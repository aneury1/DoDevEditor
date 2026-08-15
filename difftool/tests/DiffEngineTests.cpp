#include "DiffEngine.h"
#include "LogNormalizer.h"

#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testEqualIgnoringTimestamp()
{
    const jld::NormalizationOptions options {true, false, false};
    const auto left = jld::LogNormalizer::fromText(
        "2026-08-04T10:00:00+02:00 host app[1]: ready\n", options);
    const auto right = jld::LogNormalizer::fromText(
        "2026-08-04T10:05:00+02:00 host app[1]: ready\n", options);

    const auto result = jld::DiffEngine::compare(left, right);
    expect(result.summary.equalRows == 1, "timestamps should be ignored");
    expect(result.summary.differenceRows() == 0, "ignored timestamp must not create a difference");
}

void testAdditionAndRemoval()
{
    const jld::NormalizationOptions options {false, false, false};
    const auto left = jld::LogNormalizer::fromText("alpha\nbeta\ngamma\n", options);
    const auto right = jld::LogNormalizer::fromText("alpha\nnew\ngamma\nextra\n", options);

    const auto result = jld::DiffEngine::compare(left, right);
    expect(result.summary.equalRows == 2, "two lines should remain equal");
    expect(result.summary.modifiedRows == 1, "beta/new should be paired as modified");
    expect(result.summary.addedRows == 1, "extra should be a new line");
}

void testWhitespaceNormalization()
{
    const jld::NormalizationOptions options {false, true, false};
    const auto left = jld::LogNormalizer::fromText("service    started\n", options);
    const auto right = jld::LogNormalizer::fromText(" service started   \n", options);
    const auto result = jld::DiffEngine::compare(left, right);
    expect(result.summary.differenceRows() == 0, "whitespace normalization should match lines");
}

std::size_t lcsLength(const std::vector<std::string>& left, const std::vector<std::string>& right)
{
    std::vector<std::size_t> previous(right.size() + 1, 0);
    std::vector<std::size_t> current(right.size() + 1, 0);

    for (const auto& leftValue : left)
    {
        for (std::size_t column = 1; column <= right.size(); ++column)
        {
            if (leftValue == right[column - 1])
            {
                current[column] = previous[column - 1] + 1;
            }
            else
            {
                current[column] = std::max(previous[column], current[column - 1]);
            }
        }
        std::swap(previous, current);
        std::fill(current.begin(), current.end(), 0);
    }

    return previous.back();
}

std::vector<jld::SourceLine> sourceLines(const std::vector<std::string>& values)
{
    std::vector<jld::SourceLine> result;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        result.push_back(jld::SourceLine {index + 1, values[index], values[index]});
    }
    return result;
}

void testRandomizedMinimalDiff()
{
    std::mt19937 generator(123456U);
    std::uniform_int_distribution<int> lengthDistribution(0, 18);
    std::uniform_int_distribution<int> tokenDistribution(0, 5);

    for (int iteration = 0; iteration < 500; ++iteration)
    {
        std::vector<std::string> left;
        std::vector<std::string> right;
        const int leftLength = lengthDistribution(generator);
        const int rightLength = lengthDistribution(generator);

        for (int index = 0; index < leftLength; ++index)
        {
            left.push_back(std::string(1, static_cast<char>('A' + tokenDistribution(generator))));
        }
        for (int index = 0; index < rightLength; ++index)
        {
            right.push_back(std::string(1, static_cast<char>('A' + tokenDistribution(generator))));
        }

        const auto result = jld::DiffEngine::compare(sourceLines(left), sourceLines(right));
        expect(result.summary.equalRows == lcsLength(left, right), "Myers equal rows must match LCS length");

        std::vector<std::string> reconstructedLeft;
        std::vector<std::string> reconstructedRight;
        for (const auto& row : result.rows)
        {
            if (row.left)
            {
                reconstructedLeft.push_back(row.left->text);
            }
            if (row.right)
            {
                reconstructedRight.push_back(row.right->text);
            }
        }

        expect(reconstructedLeft == left, "aligned rows must preserve the complete left input");
        expect(reconstructedRight == right, "aligned rows must preserve the complete right input");
    }
}

void testEmptySides()
{
    const jld::NormalizationOptions options {};
    const auto left = jld::LogNormalizer::fromText("", options);
    const auto right = jld::LogNormalizer::fromText("one\ntwo\n", options);
    const auto result = jld::DiffEngine::compare(left, right);
    expect(result.summary.addedRows == 2, "all right-side lines should be added");
}

} // namespace

int main()
{
    testEqualIgnoringTimestamp();
    testAdditionAndRemoval();
    testWhitespaceNormalization();
    testEmptySides();
    testRandomizedMinimalDiff();
    std::cout << "All DiffEngine tests passed\n";
    return EXIT_SUCCESS;
}
