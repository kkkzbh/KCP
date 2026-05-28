package org.cp.lang.clion

import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.progress.ProgressManager
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.psi.PsiReference
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import com.intellij.psi.search.searches.ReferencesSearch
import com.intellij.util.Processor
import com.intellij.util.QueryExecutor
import java.nio.file.Path

class CpReferencesSearch : QueryExecutor<PsiReference, ReferencesSearch.SearchParameters> {
    override fun execute(queryParameters: ReferencesSearch.SearchParameters, consumer: Processor<in PsiReference>): Boolean {
        val target = queryParameters.elementToSearch
        val targetInfo = readAction {
            val declaration = target.cpNavigationTargetElement() ?: return@readAction null
            if (declaration.containingFile?.language != CpLanguage ||
                declaration.cpElementType() !in CpNavigationKinds.declarationTypes
            ) {
                return@readAction null
            }

            SearchTarget(
                path = declaration.containingFile?.virtualFile?.path?.let(::normalizeSearchPath) ?: return@readAction null,
                range = declaration.textRange.startOffset until declaration.textRange.endOffset,
                text = declaration.text,
                elementType = declaration.cpElementType(),
                functionReceiverType = declaration.cpFunctionReceiverType(),
                project = declaration.project,
                virtualFile = declaration.containingFile?.virtualFile,
            )
        } ?: return true

        val project = targetInfo.project
        val psiManager = PsiManager.getInstance(project)
        val searchScope = queryParameters.effectiveSearchScope
        val emitted = mutableSetOf<Pair<String, IntRange>>()
        val projectFiles = readAction {
            val files = linkedMapOf<String, VirtualFile>()
            targetInfo.virtualFile
                ?.takeIf { it.isValid && !it.isDirectory && searchScope.contains(it) }
                ?.let { files[normalizeSearchPath(it.path)] = it }
            for (file in FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(project))) {
                if (file.isValid && !file.isDirectory && searchScope.contains(file)) {
                    files[normalizeSearchPath(file.path)] = file
                }
            }
            files.values.toList()
        }

        for (virtualFile in projectFiles) {
            ProgressManager.checkCanceled()
            val candidateFile = readAction {
                if (!virtualFile.isValid || virtualFile.isDirectory || !searchScope.contains(virtualFile)) {
                    return@readAction null
                }

                val activeText = loadSearchText(virtualFile)
                if (!activeText.contains(targetInfo.text)) {
                    return@readAction null
                }
                CpReferenceSearchStats.recordTextCandidateFile()

                val sourceFile = psiManager.findFile(virtualFile) ?: return@readAction null
                SearchCandidateFile(
                    virtualFile = virtualFile,
                    sourceFile = sourceFile,
                    activeText = activeText,
                    fastReferences = CpReferenceCandidateFilter.candidateReferences(sourceFile, targetInfo.referenceTarget()),
                )
            } ?: continue
            candidateFile.fastReferences?.let { references ->
                when (processFastReferences(candidateFile.virtualFile, references, targetInfo, emitted, consumer)) {
                    FastReferenceResult.Complete -> continue
                    FastReferenceResult.NeedsSemantic -> {}
                    FastReferenceResult.Stop -> {
                        traceSearchDone(targetInfo, completed = false)
                        return false
                    }
                }
            }

            CpReferenceSearchStats.recordSemanticCandidateFile()
            val context = readAction {
                CpProjectSnapshotCollector.prepare(candidateFile.sourceFile, candidateFile.activeText)
            } ?: continue
            val candidateContext = SearchCandidateContext(candidateFile.virtualFile, candidateFile.sourceFile, context)
            val request = candidateContext.context.resolve()
            if (request.files.none { normalizeSearchPath(it.path) == targetInfo.path }) {
                continue
            }
            val candidate = SearchCandidate(
                virtualFile = virtualFile,
                sourceFile = candidateContext.sourceFile,
                request = request,
            )

            val result = candidate.cachedOrInspect(project) ?: continue
            for (navigation in result.navigation) {
                if (normalizeSearchPath(navigation.targetFile) != targetInfo.path ||
                    navigation.targetStartOffset != targetInfo.range.first ||
                    navigation.targetEndOffset != targetInfo.range.last + 1
                ) {
                    continue
                }

                val shouldContinue = readAction {
                    val reference = psiManager.findFile(candidate.virtualFile)
                        ?.findElementAt(navigation.sourceStartOffset)
                        ?.cpNavigationElement()
                        ?.reference
                        ?: return@readAction true
                    val key = candidate.virtualFile.path to (navigation.sourceStartOffset until navigation.sourceEndOffset)
                    !emitted.add(key) || consumer.process(reference)
                }
                if (!shouldContinue) {
                    traceSearchDone(targetInfo, completed = false)
                    return false
                }
            }
        }

        traceSearchDone(targetInfo, completed = true)
        return true
    }

    private fun processFastReferences(
        virtualFile: VirtualFile,
        references: List<PsiElement>,
        targetInfo: SearchTarget,
        emitted: MutableSet<Pair<String, IntRange>>,
        consumer: Processor<in PsiReference>,
    ): FastReferenceResult =
        readAction {
            var needsSemantic = false
            for (referenceElement in references) {
                val target = CpFastNavigationResolver.resolve(referenceElement)
                if (target == null) {
                    needsSemantic = true
                    continue
                }
                if (!target.matches(targetInfo)) {
                    continue
                }
                val reference = referenceElement.reference ?: continue
                val key = virtualFile.path to (referenceElement.textRange.startOffset until referenceElement.textRange.endOffset)
                if (emitted.add(key)) {
                    CpReferenceSearchStats.recordFastReference()
                    if (!consumer.process(reference)) {
                        return@readAction FastReferenceResult.Stop
                    }
                }
            }
            if (needsSemantic) {
                FastReferenceResult.NeedsSemantic
            } else {
                FastReferenceResult.Complete
            }
        }

    private fun loadSearchText(virtualFile: VirtualFile): String =
        FileDocumentManager.getInstance().getDocument(virtualFile)?.text
            ?: VfsUtilCore.loadText(virtualFile)

    private fun <T> readAction(block: () -> T): T =
        ApplicationManager.getApplication().runReadAction<T>(block)

    private fun traceSearchDone(targetInfo: SearchTarget, completed: Boolean) {
        val stats = CpReferenceSearchStats.snapshot()
        CpDiagnosticsTrace.info("reference-search-done:${targetInfo.path}:${targetInfo.range}:${targetInfo.text}") {
            "cp reference search done target=${targetInfo.text} completed=$completed " +
                "textCandidateFiles=${stats.textCandidateFiles} " +
                "semanticCandidateFiles=${stats.semanticCandidateFiles} fastReferences=${stats.fastReferences}"
        }
    }
}

private fun SearchCandidate.cachedOrInspect(project: com.intellij.openapi.project.Project): CpInspectionResult? {
    val cache = CpSemanticCache.get(project)
    cache.current(sourceFile)?.takeIf { it.request.sameInputAs(request) }?.let {
        return it.result
    }

    val result = CpSemanticAnalysisService.get(project).inspect(request, reason = "references") ?: return null
    cache.store(request, result, cpModificationCount(project))
    return result
}

private fun PsiElement.matches(targetInfo: SearchTarget): Boolean {
    val declaration = cpNavigationTargetElement() ?: return false
    val path = declaration.containingFile?.virtualFile?.path?.let(::normalizeSearchPath) ?: return false
    return path == targetInfo.path &&
        declaration.textRange.startOffset == targetInfo.range.first &&
        declaration.textRange.endOffset == targetInfo.range.last + 1
}

private fun CpInspectionRequest.sameInputAs(other: CpInspectionRequest): Boolean =
    activeFile == other.activeFile &&
        files.size == other.files.size &&
        files.zip(other.files).all { (left, right) -> left.path == right.path && left.text == right.text }

private fun normalizeSearchPath(path: String): String =
    runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)

private data class SearchTarget(
    val path: String,
    val range: IntRange,
    val text: String,
    val elementType: com.intellij.psi.tree.IElementType?,
    val functionReceiverType: String?,
    val project: com.intellij.openapi.project.Project,
    val virtualFile: VirtualFile?,
) {
    fun referenceTarget(): CpReferenceSearchTarget =
        CpReferenceSearchTarget(
            path = path,
            range = range,
            text = text,
            elementType = elementType,
            functionReceiverType = functionReceiverType,
        )
}

private data class SearchCandidateFile(
    val virtualFile: VirtualFile,
    val sourceFile: PsiFile,
    val activeText: String,
    val fastReferences: List<PsiElement>?,
)

private data class SearchCandidate(
    val virtualFile: VirtualFile,
    val sourceFile: PsiFile,
    val request: CpInspectionRequest,
)

private data class SearchCandidateContext(
    val virtualFile: VirtualFile,
    val sourceFile: PsiFile,
    val context: CpSnapshotContext,
)

private enum class FastReferenceResult {
    Complete,
    NeedsSemantic,
    Stop,
}

internal object CpReferenceSearchStats {
    private val textCandidateFiles = java.util.concurrent.atomic.AtomicInteger()
    private val semanticCandidateFiles = java.util.concurrent.atomic.AtomicInteger()
    private val fastReferences = java.util.concurrent.atomic.AtomicInteger()

    fun recordTextCandidateFile() {
        textCandidateFiles.incrementAndGet()
    }

    fun recordSemanticCandidateFile() {
        semanticCandidateFiles.incrementAndGet()
    }

    fun recordFastReference() {
        fastReferences.incrementAndGet()
    }

    fun reset() {
        textCandidateFiles.set(0)
        semanticCandidateFiles.set(0)
        fastReferences.set(0)
    }

    fun snapshot(): CpReferenceSearchStatsSnapshot =
        CpReferenceSearchStatsSnapshot(
            textCandidateFiles = textCandidateFiles.get(),
            semanticCandidateFiles = semanticCandidateFiles.get(),
            fastReferences = fastReferences.get(),
        )
}

internal data class CpReferenceSearchStatsSnapshot(
    val textCandidateFiles: Int,
    val semanticCandidateFiles: Int,
    val fastReferences: Int,
)
