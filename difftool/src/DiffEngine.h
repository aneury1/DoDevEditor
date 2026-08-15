#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace jld
{

enum class DiffSideKind
{
    Equal,
    Removed,
    Added,
    Modified,
    Placeholder
};

struct SourceLine
{
    std::size_t number {0};
    std::string text;
    std::string comparisonText;
};

struct AlignedRow
{
    std::optional<SourceLine> left;
    std::optional<SourceLine> right;
    DiffSideKind leftKind {DiffSideKind::Placeholder};
    DiffSideKind rightKind {DiffSideKind::Placeholder};

    [[nodiscard]] bool isDifferent() const noexcept
    {
        return leftKind != DiffSideKind::Equal || rightKind != DiffSideKind::Equal;
    }
};

struct DiffSummary
{
    std::size_t equalRows {0};
    std::size_t modifiedRows {0};
    std::size_t removedRows {0};
    std::size_t addedRows {0};

    [[nodiscard]] std::size_t differenceRows() const noexcept
    {
        return modifiedRows + removedRows + addedRows;
    }
};

struct DiffResult
{
    std::vector<AlignedRow> rows;
    DiffSummary summary;
};

class DiffEngine
{
public:
    using Comparator = std::function<bool(const SourceLine&, const SourceLine&)>;

    static DiffResult compare(
        const std::vector<SourceLine>& left,
        const std::vector<SourceLine>& right,
        Comparator comparator = {});
};

} // namespace jld
