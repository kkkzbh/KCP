export module diagnostic.collector;

import std;
import source;
import diagnostic.kind;
import diagnostic.record;
import diagnostic.catalog;

export struct diagnostic_collector {
    diagnostics: vector<diagnostic>;
}

impl diagnostic_collector {
    diagnostic_collector() = default;

    report(self&, kind: diagnostic_kind, span: source_span) -> void
    {
        let info = spec(kind);
        diagnostics.push_back(diagnostic{
            .kind = kind,
            .stage = info.stage,
            .severity = info.severity,
            .message = string{info.message},
            .primary_span = span
        });
    }

    empty(self const&) -> bool
    {
        return diagnostics.empty();
    }

    size(self const&) -> usize
    {
        return diagnostics.size();
    }

    take(self&) -> vector<diagnostic>
    {
        let result = move diagnostics;
        return result;
    }
}
