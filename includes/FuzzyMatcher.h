#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <vector>
#include <unordered_map>

struct FuzzyResult
{
    std::string id;
    std::string title;
    int score;
};

class FuzzyMatcher
{
public:
    static int Score(const std::string& text,
                     const std::string& query)
    {
        int score = 0;
        size_t j = 0;

        for (char c : query)
        {
            auto pos = text.find(c, j);
            if (pos == std::string::npos)
                return 0;

            score += 10 - (pos - j);
            j = pos + 1;
        }

        return score;
    }

    static std::vector<FuzzyResult> Search(
        const std::string& query,
        const std::unordered_map<std::string, std::string>& items)
    {
        std::vector<FuzzyResult> results;

        for (auto& [id, title] : items)
        {
            int score = Score(title, query);
            if (score > 0)
                results.push_back({id, title, score});
        }

        std::sort(results.begin(), results.end(),
            [](auto& a, auto& b)
            {
                return a.score > b.score;
            });

        return results;
    }
};