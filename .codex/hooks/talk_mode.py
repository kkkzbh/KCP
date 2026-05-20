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

用户希望先讨论具体实现方案，不要直接上手修改项目文件、做命令式重构或提交代码。你需要仔细阅读相关上下文，必要时做调试性或探索性验证，再围绕好的实现架构进行讨论。

处理规则：
1. 不要修改源代码、文档、配置等项目文件；不要生成补丁、格式化文件、stage、commit 或做持久化重构。
2. 可以读取代码、搜索调用关系、运行测试/构建/调试命令来收集证据；这些探索性命令允许产生构建产物、缓存、日志或其他临时工作区状态。
3. 运行可能改变工作区状态的探索命令前，先简短说明目的和可能产生的临时文件；结束后报告产生了什么，并在合理范围内清理或说明如何清理。
4. 如果验证某个想法需要临时实验代码，优先使用临时目录、一次性脚本、命令行输入或未纳入项目的 scratch 文件；不要把实验改动留在项目源文件里。
5. 重点分析实现边界、模块关系、数据结构、类型约束、测试策略、风险与取舍。
6. 给出你推荐的实现方案，并说明为什么它比其他备选更适合当前代码库。
7. 如果用户随后明确要求“执行”“实现”“修改”等，再进入正常改动流程。
8. 对 /home/kkkzbh/code/cp 项目，继续遵守 AGENTS.md 和 .codex/skills/cp-code-style/SKILL.md 的要求。

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
