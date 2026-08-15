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
                     const std::string& query);

    static std::vector<FuzzyResult> Search(
        const std::string& query,
        const std::unordered_map<std::string, std::string>& items);
};