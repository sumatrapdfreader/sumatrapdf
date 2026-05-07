# Bridge Contract (Shell <-> Native)

This contract defines how a modern shell layer invokes native core actions.

## Command Envelope
```json
{
  "id": "uuid",
  "type": "command",
  "name": "openFile",
  "payload": {},
  "ts": 0
}
```

## Core Commands
- openFile { path }
- goToPage { page }
- zoom { value }
- setFitMode { mode: "page-width" | "page" | "actual-size" }
- search { query, direction }
- toggleSidebar {}
- setViewMode { mode: "continuous" | "single-page" }
- addAnnotation { page, startSpanIndex, endSpanIndex, quote, comment }
- editAnnotation { id, comment }
- deleteAnnotation { id }

## Events
- documentLoaded { name, totalPages }
- pageChanged { page }
- searchResult { current, total }
- annotationChanged { total }
- performance { phase, elapsedMs }
- error { code, message }

## Reliability
- Commands must be idempotent where possible.
- Every command emits success/error completion.
- Unknown commands return error without crashing host.
