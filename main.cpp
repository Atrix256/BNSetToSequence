#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <array>
#include <direct.h>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

static const int c_imageSize = 128;

typedef std::array<float, 2> float2;

static const bool c_writeSetImage = false;
static const bool c_writeSequenceImage = true;
static const bool c_writeSequenceText = true;


// https://blog.demofox.org/2017/10/01/calculating-the-distance-between-points-in-wrap-around-toroidal-space/
float DistanceWrap(const float2& A, const float2& B)
{
	float dx = std::abs(A[0] - B[0]);
	dx = std::min(dx, 1.0f - dx);
	float dy = std::abs(A[1] - B[1]);
	dy = std::min(dy, 1.0f - dy);
	return std::sqrt(dx * dx + dy * dy);
}

void DrawPointsAsImage(const std::vector<float2>& points, int pointCount, const char* fileName)
{
	std::vector<unsigned char> pixels(c_imageSize * c_imageSize * 3, 255);

	for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)
	{
		int x = std::min(int(points[pointIndex][0] * float(c_imageSize)), c_imageSize - 1);
		int y = std::min(int(points[pointIndex][1] * float(c_imageSize)), c_imageSize - 1);

		pixels[(y * c_imageSize + x) * 3 + 0] = (pointIndex + 1 == pointCount) ? 255 : 0;
		pixels[(y * c_imageSize + x) * 3 + 1] = 0;
		pixels[(y * c_imageSize + x) * 3 + 2] = 0;
	}

	stbi_write_png(fileName, c_imageSize, c_imageSize, 3, pixels.data(), 0);
}

int main(int argc, char** argv)
{
	_mkdir("out");

	const char* inputFileNameBase = "BNOT/ours_init_ps_1024pts_%i.dat";
	const char* outputImageFileNameBase = "out/BNOT_%i_%i";
	const char* outputTextFileNameBase = "out/BNOT_%i.txt";
	int firstImage = 1;
	int lastImage = 1; // TODO: 10

	// for each noise file
	for (int imageIndex = firstImage; imageIndex <= lastImage; ++imageIndex)
	{
		printf("Image %i / %i\n", imageIndex, lastImage);

		// read the points in
		printf("  reading file\n");
		std::vector<float2> points;
		{
			char fileName[1024];
			sprintf_s(fileName, inputFileNameBase, imageIndex);

			FILE* file = nullptr;
			fopen_s(&file, fileName, "rb");

			std::vector<float> rawFloats;
			float f;
			while (fscanf_s(file, "%f", &f) == 1)
				rawFloats.push_back(f);

			fclose(file);

			points.resize(rawFloats.size() / 2);
			for (int pointIndex = 0; pointIndex < rawFloats.size() / 2; ++pointIndex)
				points[pointIndex] = float2{ rawFloats[pointIndex * 2 + 0], rawFloats[pointIndex * 2 + 1] };
		}

		// write the points out as an image		
		if (c_writeSetImage)
		{
			printf("  saving set images\n");
			for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
			{
				char fileName[1024];
				sprintf_s(fileName, outputImageFileNameBase, imageIndex, pointIndex);
				strcat_s(fileName, ".set.png");
				DrawPointsAsImage(points, pointIndex + 1, fileName);
			}
		}

		/*
		// make the points into a sequence instead of a set.
		// do this by finding the "worst" blue noise point (closest to closest neighbor) and putting it at the end of the sequence.
		// For ease of implementation, we are going to make the sequence in reverse order though, and then reverse it after.
		printf("  making sequence\n");
		for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
		{
			int worstCandidateIndex = 0;
			float worstCandidateScore = FLT_MAX;
			for (int candidateIndex = pointIndex; candidateIndex < points.size(); ++candidateIndex)
			{
				// the score for this candidate is the distance to the closest point
				float score = FLT_MAX;
				for (int testPointIndex = candidateIndex + 1; testPointIndex < points.size(); ++testPointIndex)
					score = std::min(score, DistanceWrap(points[candidateIndex], points[testPointIndex]));

				// the worst candidate is the one with the smallest distance to it's neighbor
				if (score < worstCandidateScore)
				{
					worstCandidateScore = score;
					worstCandidateIndex = candidateIndex;
				}
			}

			// put this candidate at the next step in the sequence
			std::swap(points[pointIndex], points[worstCandidateIndex]);
		}
		std::reverse(points.begin(), points.end());
		*/

		/*
		// Make the points into a sequence instead of a set.
		// Do this by starting with one of the points and doing mitchell's best candidate with the remaining points.
		// We make the first point in the point set be the first point in the sequence
		printf("  making sequence\n");
		{
			for (int pointIndex = 1; pointIndex < points.size(); ++pointIndex)
			{
				int bestCandidateIndex = 0;
				float bestCandidateScore = 0.0f;
				for (int candidateIndex = pointIndex; candidateIndex < points.size(); ++candidateIndex)
				{
					float candidateScore = FLT_MAX;
					for (int testPointIndex = 0; testPointIndex < candidateIndex; ++testPointIndex)
						candidateScore = std::min(candidateScore, DistanceWrap(points[candidateIndex], points[testPointIndex]));

					if (candidateScore > bestCandidateScore)
					{
						bestCandidateIndex = candidateIndex;
						bestCandidateScore = candidateScore;
					}
				}
				std::swap(points[pointIndex], points[bestCandidateIndex]);
			}
		}
		*/

		// Make the points into a sequence instead of a set.
		// Do this by finding the point with the highest energy and removing it.
		// We are removing the points in reverse order so will need to reverse the points when we are done.
		// This is a step from the void and cluster algorithm.
		printf("  making sequence\n");
		for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
		{
			int worstCandidateIndex = 0;
			float worstCandidateEnergy = 0.0f;
			for (int candidateIndex = pointIndex; candidateIndex < points.size(); ++candidateIndex)
			{
				float candidateEnergy = 0.0f;
				for (int testPointIndex = pointIndex; testPointIndex < points.size(); ++testPointIndex)
				{
					float d = DistanceWrap(points[candidateIndex], points[testPointIndex]);
					candidateEnergy += std::exp(-(d * d) / 2.0f); // sigma 1.0 gaussian energy function
				}

				if (candidateEnergy > worstCandidateEnergy)
				{
					worstCandidateEnergy = candidateEnergy;
					worstCandidateIndex = candidateIndex;
				}
			}

			std::swap(points[pointIndex], points[worstCandidateIndex]);
		}
		std::reverse(points.begin(), points.end());

		// write the points out as an image
		if (c_writeSequenceImage)
		{
			printf("  saving sequence images\n");

			for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex)
			{
				char fileName[1024];
				sprintf_s(fileName, outputImageFileNameBase, imageIndex, pointIndex);
				strcat_s(fileName, ".sequence.png");
				DrawPointsAsImage(points, pointIndex + 1, fileName);
			}
		}

		// write the set to a text file
		if (c_writeSequenceText)
		{
			printf("  saving sequence text\n");

			FILE* file = nullptr;
			char fileName[1024];
			sprintf_s(fileName, outputTextFileNameBase, imageIndex);

			fopen_s(&file, fileName, "wb");

			for (const float2& point : points)
				fprintf(file, "%f, %f\n", point[0], point[1]);

			fclose(file);
		}
	}

	return 0;
}

/*

TODO:
* combine the images together, make a gif, and then blog post
* give will 10 point sets from this, that are the first 32 in the sequence
* Maybe will should take all these points as candidates and pick the best 32 of em??

NOTE:
* just because BNOT is a great blue noise SET, doesn't mean that turning it into a blue noise sequence would make it better than other blue noise sequences.

* algorithm to "find the point that has the closest neighbor and make that be the next point in the reverse sequence" made holes in the blue noise. it was not very nicely progressive.
* a better algorithm might be to make voronoi cells, and remove the point with the smallest cell?
* a better algorithm is probably to do MBC forward... pick a starting one at random, and then choose whichever is farthest from existing points
 * which point to start with though? maybe should try em all? how to evaluate which one was the best to start with though? evaluating blue noise quality objectively is hard.
* Maybe will should take all these points as candidates and pick the best 32 of em??
! doing void and cluster method... energy between points. remove the point with the highest energy.

*/