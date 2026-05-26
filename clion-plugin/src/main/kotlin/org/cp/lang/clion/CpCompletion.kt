package org.cp.lang.clion

import com.intellij.codeInsight.completion.CompletionContributor
import com.intellij.codeInsight.completion.CompletionParameters
import com.intellij.codeInsight.completion.CompletionProvider
import com.intellij.codeInsight.completion.CompletionResultSet
import com.intellij.codeInsight.completion.CompletionType
import com.intellij.codeInsight.lookup.LookupElementBuilder
import com.intellij.openapi.util.Key
import com.intellij.patterns.PlatformPatterns
import com.intellij.psi.PsiFile
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
        "forward",
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

    fun items(file: PsiFile, offset: Int): List<CpCompletionItem> {
        val text = file.completionText()
        val importNameContext = looksLikeImportName(text, offset)
        val memberAccessContext = looksLikeMemberAccess(text, offset)
        val indexed = CpCompletionItemIndex.get(file)
        val items = mutableListOf<CpCompletionItem>()
        val seen = linkedSetOf<String>()

        fun add(item: CpCompletionItem) {
            if (item.name.isBlank() || !seen.add(item.name)) {
                return
            }
            items += item
        }

        fun add(name: String, tail: String? = null, type: String? = null) {
            add(CpCompletionItem(name, tail, type))
        }

        if (importNameContext) {
            for (item in indexed.imports) {
                add(item)
            }
            return items
        }

        if (memberAccessContext) {
            for (item in indexed.members) {
                add(item)
            }
            return items
        }

        for (keyword in keywords) {
            add(keyword)
        }
        val activeSymbols = CpFileSymbolIndex.get(file)
        for (parameter in activeSymbols.parameters) {
            if (parameter.offset < offset) {
                add(parameter.name, type = "参数")
            }
        }
        for (local in activeSymbols.locals) {
            if (local.offset < offset) {
                add(local.name, type = "局部变量")
            }
        }
        for (item in indexed.globals) {
            add(item)
        }
        return items
    }

    private fun looksLikeImportName(text: CharSequence, offset: Int): Boolean {
        val end = offset.coerceIn(0, text.length)
        var lineStart = end
        while (lineStart > 0 && text[lineStart - 1] != '\n') {
            lineStart -= 1
        }

        var index = skipWhitespace(text, lineStart, end)
        if (startsWithKeyword(text, index, end, "export")) {
            index += "export".length
            if (index >= end || !text[index].isWhitespace()) {
                return false
            }
            index = skipWhitespace(text, index, end)
        }

        if (!startsWithKeyword(text, index, end, "import")) {
            return false
        }
        index += "import".length
        if (index >= end || !text[index].isWhitespace()) {
            return false
        }
        index = skipWhitespace(text, index, end)
        while (index < end) {
            val char = text[index]
            if (!char.isLetterOrDigit() && char != '_' && char != '.') {
                return false
            }
            index += 1
        }
        return true
    }

    private fun looksLikeMemberAccess(text: CharSequence, offset: Int): Boolean {
        var index = offset.coerceAtMost(text.length) - 1
        while (index >= 0 && (text[index].isLetterOrDigit() || text[index] == '_')) {
            index -= 1
        }
        return index >= 0 && text[index] == '.'
    }

    private fun skipWhitespace(text: CharSequence, start: Int, end: Int): Int {
        var index = start
        while (index < end && text[index].isWhitespace()) {
            index += 1
        }
        return index
    }

    private fun startsWithKeyword(text: CharSequence, start: Int, end: Int, keyword: String): Boolean {
        if (start + keyword.length > end) {
            return false
        }
        for (index in keyword.indices) {
            if (text[start + index] != keyword[index]) {
                return false
            }
        }
        val after = start + keyword.length
        return after == end || (!text[after].isLetterOrDigit() && text[after] != '_')
    }
}

private object CpCompletionItemIndex {
    fun get(file: PsiFile): CpIndexedCompletionItems {
        val snapshot = CpSymbolScope.snapshot(file)
        val activeEntry = snapshot.entries.firstOrNull()
        val importedEntries = snapshot.entries.drop(1)

        val activeItems = activeEntry?.let { active(file, it) } ?: CpIndexedCompletionItems.empty
        val importedItems = imported(file, importedEntries)
        return activeItems.merge(importedItems)
    }

    private fun active(file: PsiFile, entry: CpSymbolScopeEntry): CpIndexedCompletionItems {
        val signature = listOf(entry.stamp)
        file.getUserData(activeCacheKey)?.takeIf { it.signature == signature }?.let {
            return it.items
        }

        val items = build(listOf(entry))
        file.putUserData(activeCacheKey, CachedCompletionItems(signature, items))
        return items
    }

    private fun imported(file: PsiFile, entries: List<CpSymbolScopeEntry>): CpIndexedCompletionItems {
        val signature = entries.map { it.stamp }
        file.getUserData(importedCacheKey)?.takeIf { it.signature == signature }?.let {
            return it.items
        }

        val items = build(entries)
        file.putUserData(importedCacheKey, CachedCompletionItems(signature, items))
        return items
    }

    private fun build(entries: List<CpSymbolScopeEntry>): CpIndexedCompletionItems =
        CpIndexedCompletionItems(
            imports = dedupe {
                for (entry in entries) {
                    for (module in entry.symbols.modules) {
                        add(CpCompletionItem(module.name, type = "模块"))
                    }
                    for (importName in entry.symbols.imports) {
                        add(CpCompletionItem(importName.name, type = "模块"))
                    }
                }
            },
            members = dedupe {
                for (entry in entries) {
                    for (field in entry.symbols.structFields) {
                        add(CpCompletionItem(field.name, type = "字段"))
                    }
                    for (function in entry.symbols.functions) {
                        if (function.receiverType != null) {
                            add(CpCompletionItem(function.name, tail = "()", type = "方法"))
                        }
                    }
                }
            },
            globals = dedupe {
                for (entry in entries) {
                    for (function in entry.symbols.functions) {
                        add(CpCompletionItem(function.name, tail = "()", type = "函数"))
                    }
                    for (type in entry.symbols.typeDeclarations) {
                        add(CpCompletionItem(type.name, type = "类型"))
                    }
                    for (caseName in entry.symbols.variantCases) {
                        add(CpCompletionItem(caseName.name, type = "变体分支"))
                    }
                    for (caseName in entry.symbols.enumCases) {
                        add(CpCompletionItem(caseName.name, type = "枚举项"))
                    }
                }
            },
        )

    private fun dedupe(build: MutableList<CpCompletionItem>.() -> Unit): List<CpCompletionItem> {
        val items = mutableListOf<CpCompletionItem>()
        items.build()
        val seen = linkedSetOf<String>()
        return items.filter { item -> item.name.isNotBlank() && seen.add(item.name) }
    }

    private val activeCacheKey = Key.create<CachedCompletionItems>("org.cp.lang.clion.activeCompletionItemIndex")
    private val importedCacheKey = Key.create<CachedCompletionItems>("org.cp.lang.clion.importedCompletionItemIndex")
}

private data class CpIndexedCompletionItems(
    val imports: List<CpCompletionItem>,
    val members: List<CpCompletionItem>,
    val globals: List<CpCompletionItem>,
) {
    fun merge(imported: CpIndexedCompletionItems): CpIndexedCompletionItems =
        CpIndexedCompletionItems(
            imports = imports.merge(imported.imports),
            members = members.merge(imported.members),
            globals = globals.merge(imported.globals),
        )

    companion object {
        val empty = CpIndexedCompletionItems(
            imports = emptyList(),
            members = emptyList(),
            globals = emptyList(),
        )
    }
}

private fun List<CpCompletionItem>.merge(other: List<CpCompletionItem>): List<CpCompletionItem> {
    val seen = linkedSetOf<String>()
    val items = mutableListOf<CpCompletionItem>()
    for (item in this + other) {
        if (item.name.isNotBlank() && seen.add(item.name)) {
            items += item
        }
    }
    return items
}

private data class CachedCompletionItems(
    val signature: List<CpSymbolScopeStamp>,
    val items: CpIndexedCompletionItems,
)

private fun PsiFile.completionText(): CharSequence =
    viewProvider.document?.charsSequence ?: text
