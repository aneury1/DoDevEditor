#include "DiffEngine.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace jld
{
namespace
{

enum class EditKind
{
    Equal,
    Delete,
    Insert
};

struct Edit
{
    EditKind kind;
    std::optional<std::size_t> leftIndex;
    std::optional<std::size_t> rightIndex;
};

std::vector<Edit> myersEdits(
    const std::vector<SourceLine>& left,
    const std::vector<SourceLine>& right,
    const DiffEngine::Comparator& equals)
{
    const int n = static_cast<int>(left.size());
    const int m = static_cast<int>(right.size());
    const int maximum = n + m;

    // Each depth stores only its active diagonal range [-depth, depth].
    // This avoids copying an O(N+M) frontier for every edit depth.
    std::vector<std::vector<int>> trace;
    trace.reserve(static_cast<std::size_t>(maximum + 1));

    const auto layerValue = [](const std::vector<int>& layer, int depth, int diagonal)
    {
        if (diagonal < -depth || diagonal > depth)
        {
            return 0;
        }
        return layer[static_cast<std::size_t>(diagonal + depth)];
    };

    int finalDepth = 0;
    bool completed = false;

    for (int depth = 0; depth <= maximum; ++depth)
    {
        std::vector<int> current(static_cast<std::size_t>(2 * depth + 1), 0);
        const std::vector<int>* previous = depth == 0 ? nullptr : &trace.back();

        for (int diagonal = -depth; diagonal <= depth; diagonal += 2)
        {
            int x = 0;
            if (depth > 0)
            {
                const int down = layerValue(*previous, depth - 1, diagonal + 1);
                const int right = layerValue(*previous, depth - 1, diagonal - 1) + 1;

                if (diagonal == -depth || (diagonal != depth && right - 1 < down))
                {
                    x = down;
                }
                else
                {
                    x = right;
                }
            }

            int y = x - diagonal;
            while (x < n && y < m &&
                   equals(left[static_cast<std::size_t>(x)], right[static_cast<std::size_t>(y)]))
            {
                ++x;
                ++y;
            }

            current[static_cast<std::size_t>(diagonal + depth)] = x;

            if (x >= n && y >= m)
            {
                finalDepth = depth;
                completed = true;
                break;
            }
        }

        trace.push_back(std::move(current));
        if (completed)
        {
            break;
        }
    }

    if (!completed)
    {
        throw std::runtime_error("Unable to calculate the line difference");
    }

    int x = n;
    int y = m;
    std::vector<Edit> reversed;
    reversed.reserve(static_cast<std::size_t>(n + m));

    for (int depth = finalDepth; depth > 0; --depth)
    {
        const auto& previous = trace[static_cast<std::size_t>(depth - 1)];
        const int diagonal = x - y;

        const int fromDelete = layerValue(previous, depth - 1, diagonal - 1);
        const int fromInsert = layerValue(previous, depth - 1, diagonal + 1);

        int previousDiagonal = 0;
        if (diagonal == -depth ||
            (diagonal != depth && fromDelete < fromInsert))
        {
            previousDiagonal = diagonal + 1;
        }
        else
        {
            previousDiagonal = diagonal - 1;
        }

        const int previousX = layerValue(previous, depth - 1, previousDiagonal);
        const int previousY = previousX - previousDiagonal;

        while (x > previousX && y > previousY)
        {
            --x;
            --y;
            reversed.push_back(Edit {
                EditKind::Equal,
                static_cast<std::size_t>(x),
                static_cast<std::size_t>(y)
            });
        }

        if (x == previousX)
        {
            --y;
            reversed.push_back(Edit {
                EditKind::Insert,
                std::nullopt,
                static_cast<std::size_t>(y)
            });
        }
        else
        {
            --x;
            reversed.push_back(Edit {
                EditKind::Delete,
                static_cast<std::size_t>(x),
                std::nullopt
            });
        }
    }

    while (x > 0 && y > 0)
    {
        --x;
        --y;
        reversed.push_back(Edit {
            EditKind::Equal,
            static_cast<std::size_t>(x),
            static_cast<std::size_t>(y)
        });
    }

    while (x > 0)
    {
        --x;
        reversed.push_back(Edit {EditKind::Delete, static_cast<std::size_t>(x), std::nullopt});
    }

    while (y > 0)
    {
        --y;
        reversed.push_back(Edit {EditKind::Insert, std::nullopt, static_cast<std::size_t>(y)});
    }

    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

void appendDifferenceBlock(
    DiffResult& result,
    const std::vector<SourceLine>& deleted,
    const std::vector<SourceLine>& inserted)
{
    const std::size_t paired = std::min(deleted.size(), inserted.size());

    for (std::size_t index = 0; index < paired; ++index)
    {
        result.rows.push_back(AlignedRow {
            deleted[index],
            inserted[index],
            DiffSideKind::Modified,
            DiffSideKind::Modified
        });
        ++result.summary.modifiedRows;
    }

    for (std::size_t index = paired; index < deleted.size(); ++index)
    {
        result.rows.push_back(AlignedRow {
            deleted[index],
            std::nullopt,
            DiffSideKind::Removed,
            DiffSideKind::Placeholder
        });
        ++result.summary.removedRows;
    }

    for (std::size_t index = paired; index < inserted.size(); ++index)
    {
        result.rows.push_back(AlignedRow {
            std::nullopt,
            inserted[index],
            DiffSideKind::Placeholder,
            DiffSideKind::Added
        });
        ++result.summary.addedRows;
    }
}

} // namespace

DiffResult DiffEngine::compare(
    const std::vector<SourceLine>& left,
    const std::vector<SourceLine>& right,
    Comparator comparator)
{
    if (!comparator)
    {
        comparator = [](const SourceLine& first, const SourceLine& second)
        {
            return first.comparisonText == second.comparisonText;
        };
    }

    const auto edits = myersEdits(left, right, comparator);
    DiffResult result;
    result.rows.reserve(std::max(left.size(), right.size()));

    std::size_t position = 0;
    while (position < edits.size())
    {
        const Edit& edit = edits[position];
        if (edit.kind == EditKind::Equal)
        {
            result.rows.push_back(AlignedRow {
                left[*edit.leftIndex],
                right[*edit.rightIndex],
                DiffSideKind::Equal,
                DiffSideKind::Equal
            });
            ++result.summary.equalRows;
            ++position;
            continue;
        }

        std::vector<SourceLine> deleted;
        std::vector<SourceLine> inserted;

        while (position < edits.size() && edits[position].kind != EditKind::Equal)
        {
            const Edit& blockEdit = edits[position];
            if (blockEdit.kind == EditKind::Delete)
            {
                deleted.push_back(left[*blockEdit.leftIndex]);
            }
            else if (blockEdit.kind == EditKind::Insert)
            {
                inserted.push_back(right[*blockEdit.rightIndex]);
            }
            ++position;
        }

        appendDifferenceBlock(result, deleted, inserted);
    }

    return result;
}

} // namespace jld
