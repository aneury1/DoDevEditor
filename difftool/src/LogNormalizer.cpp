#include "LogNormalizer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace jld
{
namespace
{

std::string removeUtf8Bom(std::string input)
{
    constexpr unsigned char bom[] {0xEF, 0xBB, 0xBF};
    if (input.size() >= 3 &&
        static_cast<unsigned char>(input[0]) == bom[0] &&
        static_cast<unsigned char>(input[1]) == bom[1] &&
        static_cast<unsigned char>(input[2]) == bom[2])
    {
        input.erase(0, 3);
    }
    return input;
}

} // namespace

std::vector<SourceLine> LogNormalizer::loadFile(
    const std::filesystem::path& path,
    const NormalizationOptions& options)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        throw std::runtime_error("Unable to open file: " + path.string());
    }

    std::string text {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()
    };

    return fromText(removeUtf8Bom(std::move(text)), options);
}

std::vector<SourceLine> LogNormalizer::fromText(
    const std::string& text,
    const NormalizationOptions& options)
{
    std::vector<SourceLine> lines;
    std::istringstream input(text);
    std::string line;
    std::size_t lineNumber = 1;

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(SourceLine {
            lineNumber,
            line,
            normalizeLine(line, options)
        });
        ++lineNumber;
    }

    // Preserve one visible empty line for an empty file.
    if (lines.empty() && !text.empty())
    {
        lines.push_back(SourceLine {1, {}, normalizeLine({}, options)});
    }

    return lines;
}

std::string LogNormalizer::normalizeLine(
    std::string line,
    const NormalizationOptions& options)
{
    if (options.ignoreTimestamp)
    {
        line = removeJournalTimestamp(line);
    }

    if (options.ignoreWhitespace)
    {
        line = normalizeWhitespace(line);
    }

    if (options.ignoreCase)
    {
        std::transform(
            line.begin(),
            line.end(),
            line.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
    }

    return line;
}

std::string LogNormalizer::removeJournalTimestamp(const std::string& line)
{
    // journalctl -o short-iso / short-iso-precise / short-full
    static const std::regex isoTimestamp(
        R"(^\s*\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:[\.,]\d+)?(?:Z|[+-]\d{2}:?\d{2})?\s+)",
        std::regex::ECMAScript);

    // journalctl -o short-full, including weekday and timezone name.
    static const std::regex fullTimestamp(
        R"(^\s*(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)\s+\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:[\.,]\d+)?\s+\S+\s+)",
        std::regex::ECMAScript);

    // journalctl -o short-unix.
    static const std::regex unixTimestamp(
        R"(^\s*\d{10,}(?:\.\d+)?\s+)",
        std::regex::ECMAScript);

    // journalctl -o short / short-precise and traditional syslog output.
    static const std::regex syslogTimestamp(
        R"(^\s*[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}(?:[\.,]\d+)?\s+)",
        std::regex::ECMAScript);

    // journalctl -o short-monotonic: [  123.456789] message
    static const std::regex monotonicTimestamp(
        R"(^\s*\[\s*\d+(?:\.\d+)?\]\s+)",
        std::regex::ECMAScript);

    // Optional generic bracketed ISO time used by some exported services.
    static const std::regex bracketedIsoTimestamp(
        R"(^\s*\[\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:[\.,]\d+)?(?:Z|[+-]\d{2}:?\d{2})?\]\s*)",
        std::regex::ECMAScript);

    std::string result = std::regex_replace(line, bracketedIsoTimestamp, "", std::regex_constants::format_first_only);
    result = std::regex_replace(result, fullTimestamp, "", std::regex_constants::format_first_only);
    result = std::regex_replace(result, isoTimestamp, "", std::regex_constants::format_first_only);
    result = std::regex_replace(result, unixTimestamp, "", std::regex_constants::format_first_only);
    result = std::regex_replace(result, syslogTimestamp, "", std::regex_constants::format_first_only);
    result = std::regex_replace(result, monotonicTimestamp, "", std::regex_constants::format_first_only);
    return result;
}

std::string LogNormalizer::normalizeWhitespace(const std::string& line)
{
    std::string result;
    result.reserve(line.size());

    bool previousWasWhitespace = false;
    for (unsigned char character : line)
    {
        const bool isWhitespace = std::isspace(character) != 0;
        if (isWhitespace)
        {
            if (!result.empty() && !previousWasWhitespace)
            {
                result.push_back(' ');
            }
        }
        else
        {
            result.push_back(static_cast<char>(character));
        }
        previousWasWhitespace = isWhitespace;
    }

    if (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }

    return result;
}

} // namespace jld
