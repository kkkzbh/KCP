export module diagnostic.record;

import std;
import source;
import diagnostic.kind;
import diagnostic.stage;
import diagnostic.severity;

export struct diagnostic_spec {
    stage: diagnostic_stage;
    severity: diagnostic_severity;
    name: str;
    message: str;
}

export struct diagnostic {
    kind: diagnostic_kind;
    stage: diagnostic_stage;
    severity: diagnostic_severity;
    message: string;
    primary_span: source_span;
}

impl diagnostic {
    diagnostic()
    {
        return diagnostic{
            .kind = diagnostic_kind::invalid_character,
            .stage = diagnostic_stage::lexer,
            .severity = diagnostic_severity::error,
            .message = string{},
            .primary_span = source_span{}
        };
    }
}
