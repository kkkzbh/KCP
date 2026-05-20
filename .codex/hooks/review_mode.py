#!/usr/bin/env python3
import json
import sys


MARKER = "::review"


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError as error:
        print(
            json.dumps(
                {
                    "systemMessage": f"review hook ignored invalid JSON input: {error}",
                },
                ensure_ascii=False,
            )
        )
        return 0

    prompt = payload.get("prompt")
    if not isinstance(prompt, str):
        return 0

    stripped_prompt = prompt.lstrip()
    lines = stripped_prompt.splitlines()
    if not lines or lines[0].strip() != MARKER:
        return 0

    suggestion = "\n".join(lines[1:]).strip()
    if not suggestion:
        print(
            json.dumps(
                {
                    "decision": "block",
                    "reason": "在 ::review 下一行写入需要判断的 review 建议。",
                },
                ensure_ascii=False,
            )
        )
        return 0

    additional_context = f"""当前进入 review 建议判断模式。

用户提供的是一条代码 review 建议，不要默认照做。你需要先判断建议是否正确、必要、符合当前代码库约束，再决定是否修改。

处理规则：
1. 如果建议正确且改动收益明确，按建议做最小必要修改，并运行相关检查。
2. 如果建议不正确、风险高、与项目约定冲突，说明拒绝理由，不修改代码。
3. 如果建议方向可能正确但信息不足，先检查代码与证据，再决定是否修改。
4. 对 /home/kkkzbh/code/cp 项目，继续遵守 AGENTS.md 和 .codex/skills/cp-code-style/SKILL.md 的要求。

review 建议如下：
{suggestion}
"""

    print(
        json.dumps(
            {
                "hookSpecificOutput": {
                    "hookEventName": "UserPromptSubmit",
                    "additionalContext": additional_context,
                },
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
