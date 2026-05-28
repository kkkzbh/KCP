package org.cp.lang.clion

import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import com.intellij.psi.tree.IElementType
import java.util.concurrent.atomic.AtomicInteger

internal object CpFastNavigationResolver {
    fun resolve(element: PsiElement): PsiElement? {
        val source = element.cpNavigationElement() ?: return null
        if (source.containingFile?.language != CpLanguage) {
            return null
        }
        val target = when (source.cpElementType()) {
            CpElements.IMPORT_NAME -> resolveImportName(source)
            CpElements.TYPE_NAME -> resolveTypeName(source)
            CpElements.NAME_EXPRESSION -> resolveNameExpression(source)
            else -> null
        }
        if (target != null) {
            CpFastNavigationStats.recordHit()
            CpDiagnosticsTrace.info("fast-navigation-hit:${source.containingFile?.virtualFile?.path}:${source.textRange}") {
                "cp fast navigation hit source=${source.text} type=${source.cpElementType()} " +
                    "target=${target.text} targetType=${target.cpElementType()}"
            }
        } else {
            CpFastNavigationStats.recordMiss()
            CpDiagnosticsTrace.info("fast-navigation-miss:${source.containingFile?.virtualFile?.path}:${source.textRange}") {
                "cp fast navigation miss source=${source.text} type=${source.cpElementType()}"
            }
        }
        return target
    }

    private fun resolveTypeName(source: PsiElement): PsiElement? {
        if (source.isCpTypeDeclarationName()) {
            return null
        }
        resolveGenericParameter(source)?.let { return it }
        val file = source.containingFile ?: return null
        sameFileTypeDeclaration(source, file)?.let { return it }
        val matches = visibleFiles(file)
            .flatMap { visibleFile -> CpFileSymbolIndex.get(visibleFile).typeDeclarations }
            .filter { symbol -> symbol.name == source.text }
            .distinctBy { symbol -> symbol.element.symbolKey() }
        return matches.singleOrNull()?.element
    }

    private fun resolveGenericParameter(source: PsiElement): PsiElement? {
        var current = source.parent
        while (current != null && current !is PsiFile) {
            val matches = current.directChild(CpElements.GENERIC_PARAMETER_LIST)
                ?.descendants(CpElements.GENERIC_PARAMETER_NAME)
                ?.filter { it.text == source.text }
                .orEmpty()
            if (matches.size == 1) {
                return matches.single()
            }
            current = current.parent
        }
        return null
    }

    private fun sameFileTypeDeclaration(source: PsiElement, file: PsiFile): PsiElement? {
        val matches = CpFileSymbolIndex.get(file).typeDeclarations
            .filter { symbol -> symbol.name == source.text }
        return matches.singleOrNull()?.element
    }

    private fun resolveImportName(source: PsiElement): PsiElement? {
        val file = source.containingFile ?: return null
        val matches = projectCpFiles(file)
            .flatMap { candidate -> CpFileSymbolIndex.get(candidate).modules }
            .filter { symbol -> symbol.name == source.text }
            .distinctBy { symbol -> symbol.element.symbolKey() }
        return matches.singleOrNull()?.element
    }

    private fun resolveNameExpression(source: PsiElement): PsiElement? {
        if (!source.isCpTopLevelFunctionCallReference()) {
            return null
        }
        val file = source.containingFile ?: return null

        val freeMatches = visibleFiles(file)
            .flatMap { visibleFile -> CpFileSymbolIndex.get(visibleFile).functions }
            .filter { symbol -> symbol.receiverType == null && symbol.name == source.text }
            .distinctBy { symbol -> symbol.element.symbolKey() }
        val implicitMemberMatches = sameImplMemberFunctionMatches(source, file)

        return when {
            freeMatches.size == 1 && implicitMemberMatches.isEmpty() -> freeMatches.single().element
            freeMatches.isEmpty() && implicitMemberMatches.size == 1 -> implicitMemberMatches.single().element
            else -> null
        }
    }

    private fun sameImplMemberFunctionMatches(source: PsiElement, file: PsiFile): List<CpFunctionSymbol> {
        val function = source.parentByType(CpElements.FUNCTION) ?: return emptyList()
        val scope = CpFileSymbolIndex.get(file).functionScopes[function]
        if (source.text in scope?.parameters.orEmpty() || source.text in scope?.locals.orEmpty()) {
            return emptyList()
        }

        val receiverType = source.parentByType(CpElements.IMPL_BLOCK)
            ?.directChild(CpElements.TYPE_REFERENCE)
            ?.receiverTypeName()
            ?: return emptyList()

        return CpFileSymbolIndex.get(file).functions
            .filter { symbol -> symbol.receiverType == receiverType && symbol.name == source.text }
            .distinctBy { symbol -> symbol.element.symbolKey() }
    }

    private fun visibleFiles(file: PsiFile): List<PsiFile> {
        val modules = projectCpFiles(file).moduleIndex()
        val result = linkedMapOf<String, PsiFile>()
        val queue = ArrayDeque<PsiFile>()
        fun add(candidate: PsiFile) {
            val key = candidate.virtualFile?.path ?: candidate.name
            if (result.putIfAbsent(key, candidate) == null) {
                queue.add(candidate)
            }
        }
        add(file)
        while (queue.isNotEmpty()) {
            val current = queue.removeFirst()
            val imports = CpFileSymbolIndex.get(current).imports.map { symbol -> symbol.name }
            for (moduleName in imports) {
                modules[moduleName]?.let(::add)
            }
        }
        return result.values.toList()
    }

    private fun projectCpFiles(file: PsiFile): List<PsiFile> {
        val psiManager = PsiManager.getInstance(file.project)
        val files = linkedMapOf<String, PsiFile>()
        file.virtualFile?.let { files[it.path] = file }
        for (virtualFile in FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(file.project))) {
            if (virtualFile.isValid && !virtualFile.isDirectory) {
                psiManager.findFile(virtualFile)?.let { psiFile ->
                    files[virtualFile.path] = psiFile
                }
            }
        }
        return files.values.toList()
    }

    private fun List<PsiFile>.moduleIndex(): Map<String, PsiFile> =
        asSequence()
            .flatMap { file ->
                CpFileSymbolIndex.get(file).modules.asSequence()
                    .map { symbol -> symbol.name to file }
            }
            .groupBy({ it.first }, { it.second })
            .mapValues { (_, files) -> files.distinctBy { file -> file.virtualFile?.path ?: file.name } }
            .filterValues { files -> files.size == 1 }
            .mapValues { (_, files) -> files.single() }
}

internal object CpReferenceCandidateFilter {
    fun candidateReferences(sourceFile: PsiFile, target: CpReferenceSearchTarget): List<PsiElement>? {
        return when (target.elementType) {
            CpElements.MODULE_NAME ->
                sourceFile.descendants(CpElements.IMPORT_NAME)
                    .filter { element -> element.text == target.text }
            CpElements.TYPE_NAME ->
                sourceFile.descendants(CpElements.TYPE_NAME)
                    .filter { element ->
                        element.text == target.text &&
                            !element.isCpTypeDeclarationName()
                    }
            CpElements.FUNCTION_NAME ->
                if (target.functionReceiverType == null) {
                    sourceFile.descendants(CpElements.NAME_EXPRESSION)
                        .filter { element ->
                            element.text == target.text &&
                                element.isCpTopLevelFunctionCallReference()
                        }
                } else {
                    null
                }
            else -> null
        }
    }
}

internal object CpFastNavigationStats {
    private val hits = AtomicInteger()
    private val misses = AtomicInteger()

    fun recordHit() {
        hits.incrementAndGet()
    }

    fun recordMiss() {
        misses.incrementAndGet()
    }

    fun reset() {
        hits.set(0)
        misses.set(0)
    }

    fun snapshot(): CpFastNavigationSnapshot =
        CpFastNavigationSnapshot(
            hits = hits.get(),
            misses = misses.get(),
        )
}

internal data class CpFastNavigationSnapshot(
    val hits: Int,
    val misses: Int,
)

internal fun PsiElement.isCpTypeDeclarationName(): Boolean =
    cpElementType() == CpElements.TYPE_NAME &&
        parent?.cpElementType() in CpTypeDeclarationParents &&
        parent?.directChild(CpElements.TYPE_NAME) == this

internal val CpTypeDeclarationParents = setOf(
    CpElements.STRUCT_DECLARATION,
    CpElements.ENUM_DECLARATION,
    CpElements.VARIANT_DECLARATION,
    CpElements.CONCEPT_DECLARATION,
    CpElements.TYPE_ALIAS,
)

internal data class CpReferenceSearchTarget(
    val path: String,
    val range: IntRange,
    val text: String,
    val elementType: IElementType?,
    val functionReceiverType: String?,
)

internal fun PsiElement.isCpTopLevelFunctionCallReference(): Boolean =
    cpElementType() == CpElements.NAME_EXPRESSION &&
        parent?.cpElementType() == CpElements.CALL_EXPRESSION

internal fun PsiElement.cpFunctionReceiverType(): String? {
    if (cpElementType() != CpElements.FUNCTION_NAME) {
        return null
    }
    val file = containingFile ?: return null
    return CpFileSymbolIndex.get(file).functions
        .singleOrNull { symbol -> symbol.element == this }
        ?.receiverType
}

private fun PsiElement.symbolKey(): Pair<String?, IntRange> =
    (containingFile?.virtualFile?.path ?: containingFile?.name) to
        (textRange.startOffset until textRange.endOffset)

private fun PsiElement.receiverTypeName(): String? =
    firstDescendant(CpElements.TYPE_NAME)
        ?.text
        ?.takeWhile { it.isLetterOrDigit() || it == '_' }
        ?.takeIf { it.isNotEmpty() }
