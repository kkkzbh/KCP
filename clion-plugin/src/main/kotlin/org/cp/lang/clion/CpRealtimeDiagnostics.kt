package org.cp.lang.clion

import com.intellij.execution.RunManager
import com.intellij.execution.RunManagerListener
import com.intellij.execution.RunnerAndConfigurationSettings
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.editor.EditorFactory
import com.intellij.openapi.editor.event.DocumentEvent
import com.intellij.openapi.editor.event.DocumentListener
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.fileEditor.FileEditorManager
import com.intellij.openapi.fileEditor.FileEditorManagerListener
import com.intellij.openapi.project.Project
import com.intellij.openapi.startup.ProjectActivity
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.util.Alarm
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong

internal class CpRealtimeDiagnosticsActivity : ProjectActivity {
    override suspend fun execute(project: Project) {
        log.info("cp plugin active implementation=${CpPlugin.IMPLEMENTATION_TAG} project=${project.name}")
        CpRealtimeDiagnosticsRefresher(project).install()
    }

    private companion object {
        private val log = Logger.getInstance(CpRealtimeDiagnosticsActivity::class.java)
    }
}

private class CpRealtimeDiagnosticsRefresher(
    private val project: Project,
) {
    private val alarm = Alarm(Alarm.ThreadToUse.SWING_THREAD, project)
    private val nextRefreshGeneration = AtomicLong()
    private val fileRefreshGenerations = ConcurrentHashMap<String, Long>()
    private val openFileRefreshGeneration = AtomicLong()

    fun install() {
        EditorFactory.getInstance().eventMulticaster.addDocumentListener(
            object : DocumentListener {
                override fun documentChanged(event: DocumentEvent) {
                    val file = FileDocumentManager.getInstance().getFile(event.document)
                        ?.takeIf { it.isValid && !it.isDirectory && it.extension == CpFileType.INSTANCE.defaultExtension }
                        ?: return
                    scheduleFileRefresh(file, typingDiagnosticsDelayMillis)
                }
            },
            project,
        )

        val connection = project.messageBus.connect(project)
        connection.subscribe(
            RunManagerListener.TOPIC,
            object : RunManagerListener {
                override fun runConfigurationSelected(settings: RunnerAndConfigurationSettings?) {
                    scheduleOpenFileRefresh(runConfigurationRefreshDelayMillis, force = true)
                }

                override fun runConfigurationChanged(settings: RunnerAndConfigurationSettings) {
                    if (settings === RunManager.getInstance(project).selectedConfiguration) {
                        scheduleOpenFileRefresh(runConfigurationRefreshDelayMillis, force = true)
                    }
                }
            },
        )
        connection.subscribe(
            FileEditorManagerListener.FILE_EDITOR_MANAGER,
            object : FileEditorManagerListener {
                override fun fileOpened(source: FileEditorManager, file: VirtualFile) {
                    if (file.isValid && !file.isDirectory && file.extension == CpFileType.INSTANCE.defaultExtension) {
                        scheduleFileRefresh(file, fileOpenRefreshDelayMillis)
                    }
                }
            },
        )
        scheduleOpenFileRefresh(initialRefreshDelayMillis)
    }

    private fun scheduleFileRefresh(file: VirtualFile, delayMillis: Int) {
        if (project.isDisposed) {
            return
        }
        val path = file.path
        val generation = nextRefreshGeneration.incrementAndGet()
        fileRefreshGenerations[path] = generation
        alarm.addRequest(
            {
                if (fileRefreshGenerations.remove(path, generation)) {
                    refreshFile(file, force = false)
                }
            },
            delayMillis,
        )
    }

    private fun scheduleOpenFileRefresh(delayMillis: Int, force: Boolean = false) {
        if (project.isDisposed) {
            return
        }
        val generation = nextRefreshGeneration.incrementAndGet()
        openFileRefreshGeneration.set(generation)
        alarm.addRequest(
            {
                if (openFileRefreshGeneration.get() == generation) {
                    refreshOpenCpFiles(force)
                }
            },
            delayMillis,
        )
    }

    private fun refreshOpenCpFiles(force: Boolean) {
        for (file in openCpFiles()) {
            refreshFile(file, force)
        }
    }

    private fun openCpFiles(): List<VirtualFile> =
        FileEditorManager.getInstance(project).openFiles
            .asSequence()
            .filter { it.isValid && !it.isDirectory && it.extension == CpFileType.INSTANCE.defaultExtension }
            .toList()

    private fun refreshFile(file: VirtualFile, force: Boolean) {
        if (project.isDisposed || !file.isValid || file.isDirectory) {
            return
        }
        val files = ApplicationManager.getApplication().runReadAction<List<PsiFile>> {
            listOfNotNull(PsiManager.getInstance(project).findFile(file))
                .filter { it.language == CpLanguage }
        }

        for (psiFile in files) {
            CpSemanticCache.get(project).requestRefresh(psiFile, force)
        }
    }

    private companion object {
        const val typingDiagnosticsDelayMillis = 1500
        const val fileOpenRefreshDelayMillis = 500
        const val initialRefreshDelayMillis = 900
        const val runConfigurationRefreshDelayMillis = 250
    }
}
