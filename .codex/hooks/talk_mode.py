#!/usr/bin/env python3
import json
import sys


MARKER = "::talk"


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError as error:
        print(
            json.dumps(
                {
                    "systemMessage": f"talk hook ignored invalid JSON input: {error}",
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

    topic = "\n".join(lines[1:]).strip()
    if not topic:
        print(
            json.dumps(
                {
                    "decision": "block",
                    "reason": "在 ::talk 下一行写入需要讨论的具体实现问题。",
                },
                ensure_ascii=False,
            )
        )
        return 0

    additional_context = f"""当前进入实现方案讨论模式。

用户希望先讨论具体实现方案，不要进行文件修改、命令式重构或直接提交代码。你需要仔细阅读相关上下文，思考好的实现架构，并以方案讨论为主。

处理规则：
1. 不要修改仓库文件，不要生成补丁，不要执行会改变工作区状态的命令。
2. 可以读取代码、运行只读检查命令来收集证据；如果某个验证命令可能写入构建产物，先说明再等待用户确认。
3. 重点分析实现边界、模块关系、数据结构、类型约束、测试策略、风险与取舍。
4. 给出你推荐的实现方案，并说明为什么它比其他备选更适合当前代码库。
5. 如果用户随后明确要求“执行”“实现”“修改”等，再进入正常改动流程。
6. 对 /home/kkkzbh/code/cp 项目，继续遵守 AGENTS.md 和 .codex/skills/cp-code-style/SKILL.md 的要求。

需要讨论的问题如下：
{topic}
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
