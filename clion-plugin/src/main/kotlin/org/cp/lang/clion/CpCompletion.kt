package org.cp.lang.clion

import com.intellij.codeInsight.completion.CompletionContributor
import com.intellij.codeInsight.completion.CompletionParameters
import com.intellij.codeInsight.completion.CompletionProvider
import com.intellij.codeInsight.completion.CompletionResultSet
import com.intellij.codeInsight.completion.CompletionType
import com.intellij.codeInsight.lookup.LookupElementBuilder
import com.intellij.patterns.PlatformPatterns
import com.intellij.util.ProcessingContext

class CpCompletionContributor : CompletionContributor() {
    init {
        extend(
            CompletionType.BASIC,
            PlatformPatterns.psiElement().withLanguage(CpLanguage),
            CpCompletionProvider,
        )
    }
}

private object CpCompletionProvider : CompletionProvider<CompletionParameters>() {
    override fun addCompletions(parameters: CompletionParameters, context: ProcessingContext, result: CompletionResultSet) {
        val file = parameters.originalFile.takeIf { it.language == CpLanguage } ?: return
        for (item in CpCompletionEngine.items(file, parameters.offset)) {
            var lookup = LookupElementBuilder.create(item.name)
            if (item.tail != null) {
                lookup = lookup.withTailText(item.tail, true)
            }
            if (item.type != null) {
                lookup = lookup.withTypeText(item.type, true)
            }
            result.addElement(lookup)
        }
    }
}

data class CpCompletionItem(
    val name: String,
    val tail: String? = null,
    val type: String? = null,
)

object CpCompletionEngine {
    private val keywords = listOf(
        "let",
        "const",
        "if",
        "else",
        "while",
        "do",
        "for",
        "break",
        "continue",
        "return",
        "import",
        "export",
        "module",
        "struct",
        "enum",
        "impl",
        "concept",
        "operator",
        "builtin",
        "prefix",
        "postfix",
        "match",
        "variant",
        "type",
        "requires",
        "extern",
        "true",
        "false",
        "nullptr",
    )

    fun items(file: com.intellij.psi.PsiFile, offset: Int): List<CpCompletionItem> {
        val text = file.text
        val items = mutableListOf<CpCompletionItem>()
        val seen = linkedSetOf<String>()

        fun add(name: String, tail: String? = null, type: String? = null) {
            if (name.isBlank() || !seen.add(name)) {
                return
            }
            items += CpCompletionItem(name, tail, type)
        }

        if (looksLikeImportName(text, offset)) {
            for (module in file.descendants(CpElements.MODULE_NAME)) {
                add(module.text, type = "模块")
            }
            for (importName in file.descendants(CpElements.IMPORT_NAME)) {
                add(importName.text, type = "模块")
            }
            return items
        }

        if (looksLikeMemberAccess(text, offset)) {
            for (field in file.descendants(CpElements.FIELD_DECLARATION)) {
                if (field.parent?.cpElementType() == CpElements.STRUCT_FIELD) {
                    add(field.text, type = "字段")
                }
            }
            for (function in file.descendants(CpElements.FUNCTION_NAME)) {
                if (function.parent?.parentByType(CpElements.IMPL_BLOCK) != null) {
                    add(function.text, tail = "()", type = "方法")
                }
            }
            return items
        }

        for (keyword in keywords) {
            add(keyword)
        }
        for (parameter in file.descendants(CpElements.PARAMETER_NAME)) {
            if (parameter.textRange.startOffset < offset) {
                add(parameter.text, type = "参数")
            }
        }
        for (local in file.descendants(CpElements.LOCAL_DECLARATION)) {
            if (local.textRange.startOffset < offset) {
                add(local.text, type = "局部变量")
            }
        }
        for (function in file.descendants(CpElements.FUNCTION_NAME)) {
            add(function.text, tail = "()", type = "函数")
        }
        for (type in file.descendants(CpElements.TYPE_NAME)) {
            if (type.isCompletionTypeDeclaration()) {
                add(type.text, type = "类型")
            }
        }
        for (caseName in file.descendants(CpElements.VARIANT_CASE_NAME)) {
            add(caseName.text, type = "变体分支")
        }
        for (caseName in file.descendants(CpElements.ENUM_CASE_NAME)) {
            add(caseName.text, type = "枚举项")
        }
        return items
    }

    private fun looksLikeImportName(text: String, offset: Int): Boolean {
        val lineStart = text.lastIndexOf('\n', (offset - 1).coerceAtLeast(0)).let { if (it < 0) 0 else it + 1 }
        val prefix = text.substring(lineStart, offset.coerceIn(lineStart, text.length))
        return Regex("""^\s*(?:export\s+)?import\s+[\w.]*$""").matches(prefix)
    }

    private fun looksLikeMemberAccess(text: String, offset: Int): Boolean {
        var index = (offset - 1).coerceAtMost(text.lastIndex)
        while (index >= 0 && (text[index].isLetterOrDigit() || text[index] == '_')) {
            index -= 1
        }
        return index >= 0 && text[index] == '.'
    }

    private fun com.intellij.psi.PsiElement.isCompletionTypeDeclaration(): Boolean =
        when (parent?.cpElementType()) {
            CpElements.STRUCT_DECLARATION,
            CpElements.ENUM_DECLARATION,
            CpElements.VARIANT_DECLARATION,
            CpElements.CONCEPT_DECLARATION,
            CpElements.TYPE_ALIAS -> parent?.firstDescendant(CpElements.TYPE_NAME) == this
            else -> false
        }
}
