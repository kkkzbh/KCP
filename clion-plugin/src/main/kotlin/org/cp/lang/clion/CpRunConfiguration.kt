package org.cp.lang.clion

import com.intellij.build.BuildViewManager
import com.intellij.build.DefaultBuildDescriptor
import com.intellij.build.events.impl.FailureResultImpl
import com.intellij.build.events.impl.SuccessResultImpl
import com.intellij.build.progress.BuildProgress
import com.intellij.build.progress.BuildProgressDescriptor
import com.intellij.build.progress.BuildProgressDescriptorImpl
import com.intellij.execution.BeforeRunTask
import com.intellij.execution.BeforeRunTaskProvider
import com.intellij.execution.CommonProgramRunConfigurationParameters
import com.intellij.execution.ExecutionException
import com.intellij.execution.Executor
import com.intellij.execution.InputRedirectAware
import com.intellij.execution.RunManager
import com.intellij.execution.actions.ConfigurationContext
import com.intellij.execution.actions.LazyRunConfigurationProducer
import com.intellij.execution.configuration.EnvironmentVariablesData
import com.intellij.execution.configurations.CommandLineState
import com.intellij.execution.configurations.ConfigurationFactory
import com.intellij.execution.configurations.ConfigurationTypeBase
import com.intellij.execution.configurations.ConfigurationTypeUtil
import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.execution.configurations.PtyCommandLine
import com.intellij.execution.configurations.RunConfiguration
import com.intellij.execution.configurations.RunConfigurationBase
import com.intellij.execution.configurations.RunConfigurationOptions
import com.intellij.execution.configurations.RunProfileState
import com.intellij.execution.configurations.RuntimeConfigurationError
import com.intellij.execution.lineMarker.ExecutorAction
import com.intellij.execution.lineMarker.RunLineMarkerContributor
import com.intellij.execution.process.CapturingProcessHandler
import com.intellij.execution.process.OSProcessHandler
import com.intellij.execution.process.ProcessOutputType
import com.intellij.execution.process.ProcessHandler
import com.intellij.execution.runners.ExecutionEnvironment
import com.intellij.execution.ui.CommonProgramParametersPanel
import com.intellij.execution.ui.ProgramInputRedirectPanel
import com.intellij.icons.AllIcons
import com.intellij.openapi.application.PathManager
import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.fileChooser.FileChooserDescriptorFactory
import com.intellij.openapi.options.SettingsEditor
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import com.intellij.openapi.util.NotNullLazyValue
import com.intellij.openapi.util.Ref
import com.intellij.openapi.roots.ProjectModelExternalSource
import com.intellij.openapi.ui.TextFieldWithBrowseButton
import com.intellij.psi.PsiElement
import com.intellij.task.ProjectModelBuildTask
import com.intellij.task.ProjectTask
import com.intellij.task.ProjectTaskContext
import com.intellij.task.ProjectTaskRunner
import com.intellij.task.TaskRunnerResults
import com.intellij.ui.RawCommandLineEditor
import com.intellij.ui.components.JBCheckBox
import com.intellij.ui.components.JBLabel
import com.intellij.util.execution.ParametersListUtil
import com.jetbrains.cidr.execution.CidrBuildConfiguration
import com.jetbrains.cidr.execution.build.CidrBuildConfigurationProvider
import com.jetbrains.cidr.execution.build.runners.CidrProjectTaskRunner
import com.jetbrains.cidr.execution.build.runners.CidrTaskRunner
import org.jdom.Element
import org.jetbrains.concurrency.Promise
import org.jetbrains.concurrency.resolvedPromise
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import java.io.File
import java.io.IOException
import java.nio.file.FileVisitResult
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.SimpleFileVisitor
import java.nio.file.attribute.BasicFileAttributes
import java.security.MessageDigest
import java.util.HexFormat
import javax.swing.JComponent
import javax.swing.JPanel

class CpRunConfigurationType : ConfigurationTypeBase(
    ID,
    "cp",
    "从 main 函数运行 cp 源文件",
    NotNullLazyValue.createValue { AllIcons.RunConfigurations.Application },
) {
    init {
        addFactory(CpRunConfigurationFactory(this))
    }

    companion object {
        const val ID: String = "CpRunConfiguration"
    }
}

class CpRunConfigurationProducer : LazyRunConfigurationProducer<CpRunConfiguration>() {
    override fun setupConfigurationFromContext(
        configuration: CpRunConfiguration,
        context: ConfigurationContext,
        sourceElement: Ref<PsiElement>,
    ): Boolean {
        val main = context.psiLocation?.cpTopLevelMainFunction() ?: return false
        val path = main.containingFile?.virtualFile?.path ?: return false

        configuration.mainFile = path
        configuration.name = main.containingFile.name.removeSuffix(".cp")
        configuration.ensureBuildBeforeRunTask()
        sourceElement.set(main.directChild(CpElements.FUNCTION_NAME) ?: main)
        return true
    }

    override fun isConfigurationFromContext(configuration: CpRunConfiguration, context: ConfigurationContext): Boolean {
        val main = context.psiLocation?.cpTopLevelMainFunction() ?: return false
        val filePath = main.containingFile?.virtualFile?.path ?: return false
        val path = Path.of(filePath).toAbsolutePath().normalize()
        val configured = configuration.mainFile.toPathOrNull()?.toAbsolutePath()?.normalize() ?: return false
        return path == configured
    }

    override fun getConfigurationFactory(): ConfigurationFactory =
        cpRunConfigurationFactory()
}

class CpRunLineMarkerContributor : RunLineMarkerContributor() {
    override fun getInfo(element: PsiElement): Info? {
        if (element.cpElementType() != CpTypes.IDENTIFIER || element.text != "main") {
            return null
        }
        val functionName = element.parent.takeIf { it?.cpElementType() == CpElements.FUNCTION_NAME } ?: return null
        val function = functionName.parent.takeIf { it?.cpTopLevelMainFunction() != null } ?: return null
        if (function.directChild(CpElements.FUNCTION_NAME) != functionName) {
            return null
        }

        return Info(
            AllIcons.RunConfigurations.TestState.Run,
            ExecutorAction.getActions(0),
        ) { "运行 '${function.containingFile.name}'" }
    }
}

private class CpRunConfigurationFactory(type: CpRunConfigurationType) : ConfigurationFactory(type) {
    override fun getId(): String = CpRunConfigurationType.ID

    override fun createTemplateConfiguration(project: Project): RunConfiguration =
        CpRunConfiguration(project, this, "main").also { it.ensureBuildBeforeRunTask() }
}

private fun cpRunConfigurationFactory(): ConfigurationFactory =
    ConfigurationTypeUtil.findConfigurationType(CpRunConfigurationType::class.java).configurationFactories.single()

class CpRunConfiguration(project: Project, factory: ConfigurationFactory, name: String) :
    RunConfigurationBase<RunConfigurationOptions>(project, factory, name),
    CommonProgramRunConfigurationParameters,
    InputRedirectAware {
    var mainFile: String = ""
    var compilerPath: String = ""
    var compileOptions: String = ""
    @get:JvmName("getCpWorkingDirectory")
    @set:JvmName("setCpWorkingDirectory")
    var workingDirectory: String = project.basePath ?: ""
    var programArguments: String = ""
    @get:JvmName("getCpPassParentEnvs")
    @set:JvmName("setCpPassParentEnvs")
    var passParentEnvs: Boolean = true
    var redirectInput: Boolean = false
    var redirectInputPath: String = ""
    var emulateTerminal: Boolean = false
    private var environmentVariables: EnvironmentVariablesData = EnvironmentVariablesData.DEFAULT
    private var buildBeforeRunTasksMigrated: Boolean = false
    internal val redirectOptions = object : InputRedirectAware.InputRedirectOptions {
        override fun isRedirectInput(): Boolean = this@CpRunConfiguration.redirectInput

        override fun setRedirectInput(value: Boolean) {
            this@CpRunConfiguration.redirectInput = value
        }

        override fun getRedirectInputPath(): String? =
            this@CpRunConfiguration.redirectInputPath.takeIf { it.isNotBlank() }

        override fun setRedirectInputPath(value: String?) {
            this@CpRunConfiguration.redirectInputPath = value.orEmpty()
        }
    }

    override fun getProgramParameters(): String = programArguments

    override fun setProgramParameters(value: String?) {
        programArguments = value.orEmpty()
    }

    override fun getWorkingDirectory(): String = workingDirectory

    override fun setWorkingDirectory(value: String?) {
        workingDirectory = value.orEmpty()
    }

    override fun getEnvs(): MutableMap<String, String> =
        environmentVariables.envs.toMutableMap()

    override fun setEnvs(value: MutableMap<String, String>) {
        environmentVariables = environmentVariables.with(value)
    }

    override fun isPassParentEnvs(): Boolean = passParentEnvs

    override fun setPassParentEnvs(value: Boolean) {
        passParentEnvs = value
        environmentVariables = environmentVariables.with(value)
    }

    override fun getConfigurationEditor(): SettingsEditor<out RunConfiguration> = CpRunConfigurationEditor(project)

    override fun checkConfiguration() {
        val source = mainFile.toPathOrNull()
            ?: throw RuntimeConfigurationError("请选择 cp main 源文件")
        if (!Files.isRegularFile(source)) {
            throw RuntimeConfigurationError("cp main 源文件不存在：$mainFile")
        }
        if (source.fileName.toString().substringAfterLast('.', "") != CpFileType.INSTANCE.defaultExtension) {
            throw RuntimeConfigurationError("cp main 源文件必须使用 .cp 扩展名")
        }
        if (CpRunPaths.resolveCompiler(project, compilerPath, source) == null) {
            throw RuntimeConfigurationError("找不到 cp 编译器；请构建编译器目标或设置编译器路径")
        }
        val resolvedWorkingDirectory = CpRunPaths.resolveWorkingDirectory(project, workingDirectory, source)
        if (!Files.isDirectory(resolvedWorkingDirectory)) {
            throw RuntimeConfigurationError("工作目录不存在：$resolvedWorkingDirectory")
        }
        validateCpParameters("编译选项", compileOptions)
        validateCpParameters("程序实参", programArguments)
        resolveRedirectInputFile(redirectInput, redirectInputPath)
    }

    override fun getState(executor: Executor, environment: ExecutionEnvironment): RunProfileState =
        CpRunCommandLineState(environment, this)

    override fun getInputRedirectOptions(): InputRedirectAware.InputRedirectOptions = redirectOptions

    override fun onNewConfigurationCreated() {
        super.onNewConfigurationCreated()
        ensureBuildBeforeRunTask()
    }

    override fun readExternal(element: Element) {
        super<RunConfigurationBase>.readExternal(element)
        mainFile = element.getAttributeValue("mainFile", "")
        compilerPath = element.getAttributeValue("compilerPath", "")
        compileOptions = element.getAttributeValue("compileOptions", "")
        workingDirectory = element.getAttributeValue("workingDirectory", project.basePath ?: "")
        programArguments = element.getAttributeValue("programArguments", "")
        environmentVariables = EnvironmentVariablesData.readExternal(element)
        passParentEnvs = environmentVariables.isPassParentEnvs
        redirectInput = element.getAttributeValue("redirectInput", "false").toBoolean()
        redirectInputPath = element.getAttributeValue("redirectInputPath", "")
        emulateTerminal = element.getAttributeValue("emulateTerminal", "false").toBoolean()
        buildBeforeRunTasksMigrated = element.getAttributeValue("buildBeforeRunTasksMigrated", "false").toBoolean()
        if (!buildBeforeRunTasksMigrated) {
            ensureBuildBeforeRunTask()
        }
    }

    override fun writeExternal(element: Element) {
        super<RunConfigurationBase>.writeExternal(element)
        element.setAttribute("mainFile", mainFile)
        element.setAttribute("compilerPath", compilerPath)
        element.setAttribute("compileOptions", compileOptions)
        element.setAttribute("workingDirectory", workingDirectory)
        element.setAttribute("programArguments", programArguments)
        environmentVariables.with(passParentEnvs).writeExternal(element)
        element.setAttribute("redirectInput", redirectInput.toString())
        element.setAttribute("redirectInputPath", redirectInputPath)
        element.setAttribute("emulateTerminal", emulateTerminal.toString())
        element.setAttribute("buildBeforeRunTasksMigrated", buildBeforeRunTasksMigrated.toString())
    }

    internal fun ensureBuildBeforeRunTask() {
        val tasks = getBeforeRunTasks().toMutableList()
        if (tasks.none { it.providerId == CpBuildBeforeRunTaskProvider.ID }) {
            tasks.add(CpBuildBeforeRunTask().also { it.isEnabled = true })
            setBeforeRunTasks(tasks)
        }
        buildBeforeRunTasksMigrated = true
    }
}

@Suppress("DEPRECATION")
private class CpRunConfigurationEditor(project: Project) : SettingsEditor<CpRunConfiguration>() {
    private val mainFileField = TextFieldWithBrowseButton()
    private val compilerPathField = TextFieldWithBrowseButton()
    private val compileOptionsField = RawCommandLineEditor()
    private val commonProgramParametersPanel = object : CommonProgramParametersPanel(true) {
        override fun getProject(): Project = project
    }
    private val redirectInputPanel = ProgramInputRedirectPanel()
    private val emulateTerminalBox = JBCheckBox("模拟终端")
    private val panel = JPanel(GridBagLayout())

    init {
        mainFileField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("选择 cp main 源文件")
                .withExtensionFilter("cp 源文件", CpFileType.INSTANCE.defaultExtension),
        )
        compilerPathField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("选择 cp 编译器"),
        )
        commonProgramParametersPanel.setProgramParametersLabel("程序实参：")
        addRow(0, "编译器路径：", compilerPathField)
        addRow(1, "主文件：", mainFileField)
        addRow(2, "编译选项：", compileOptionsField)
        addWideRow(3, commonProgramParametersPanel)
        addWideRow(4, redirectInputPanel)
        addRow(5, "其他选项：", emulateTerminalBox)
    }

    override fun resetEditorFrom(configuration: CpRunConfiguration) {
        mainFileField.text = configuration.mainFile
        compilerPathField.text = configuration.compilerPath
        compileOptionsField.text = configuration.compileOptions
        commonProgramParametersPanel.reset(configuration)
        redirectInputPanel.reset(configuration.inputRedirectOptions)
        emulateTerminalBox.isSelected = configuration.emulateTerminal
    }

    override fun applyEditorTo(configuration: CpRunConfiguration) {
        configuration.mainFile = mainFileField.text.trim()
        configuration.compilerPath = compilerPathField.text.trim()
        configuration.compileOptions = compileOptionsField.text.trim()
        commonProgramParametersPanel.applyTo(configuration)
        redirectInputPanel.applyTo(configuration.inputRedirectOptions)
        configuration.emulateTerminal = emulateTerminalBox.isSelected
    }

    override fun createEditor(): JComponent = panel

    private fun addRow(row: Int, label: String, component: JComponent) {
        panel.add(
            JBLabel(label),
            GridBagConstraints().apply {
                gridx = 0
                gridy = row
                anchor = GridBagConstraints.WEST
                insets.set(4, 0, 4, 8)
            },
        )
        panel.add(
            component,
            GridBagConstraints().apply {
                gridx = 1
                gridy = row
                weightx = 1.0
                fill = GridBagConstraints.HORIZONTAL
                insets.set(4, 0, 4, 0)
            },
        )
    }

    private fun addWideRow(row: Int, component: JComponent) {
        panel.add(
            component,
            GridBagConstraints().apply {
                gridx = 0
                gridy = row
                gridwidth = 2
                weightx = 1.0
                fill = GridBagConstraints.HORIZONTAL
                insets.set(4, 0, 4, 0)
            },
        )
    }
}

private val CP_BUILD_BEFORE_RUN_TASK_ID: Key<CpBuildBeforeRunTask> = Key.create("org.cp.lang.clion.CpBuildBeforeRunTask")

class CpBuildBeforeRunTask : BeforeRunTask<CpBuildBeforeRunTask>(CP_BUILD_BEFORE_RUN_TASK_ID)

class CpBuildBeforeRunTaskProvider : BeforeRunTaskProvider<CpBuildBeforeRunTask>() {
    override fun getId(): Key<CpBuildBeforeRunTask> = ID

    override fun getName(): String = "构建"

    override fun getIcon() = AllIcons.Actions.Compile

    override fun createTask(runConfiguration: RunConfiguration): CpBuildBeforeRunTask? =
        (runConfiguration as? CpRunConfiguration)?.let { CpBuildBeforeRunTask() }

    override fun canExecuteTask(configuration: RunConfiguration, task: CpBuildBeforeRunTask): Boolean =
        configuration is CpRunConfiguration

    override fun executeTask(
        context: com.intellij.openapi.actionSystem.DataContext,
        configuration: RunConfiguration,
        environment: ExecutionEnvironment,
        task: CpBuildBeforeRunTask,
    ): Boolean =
        (configuration as? CpRunConfiguration)?.runCpBuild() ?: false

    companion object {
        val ID: Key<CpBuildBeforeRunTask> = CP_BUILD_BEFORE_RUN_TASK_ID
    }
}

class CpBuildConfigurationProvider : CidrBuildConfigurationProvider {
    override fun getBuildableConfigurations(project: Project): List<CidrBuildConfiguration> {
        val configuration = RunManager.getInstance(project).selectedConfiguration?.configuration as? CpRunConfiguration
            ?: return emptyList()
        return listOf(CpBuildConfiguration(configuration))
    }
}

class CpProjectTaskRunner : CidrProjectTaskRunner() {
    override val buildSystemId: String = "CpRunFile"

    override fun canRun(projectTask: ProjectTask): Boolean =
        projectTask.cpBuildConfiguration() != null

    override fun runnerForTask(task: ProjectTask, project: Project): CidrTaskRunner? =
        if (task.cpBuildConfiguration() != null) CpBuildTaskRunner() else null
}

private class CpBuildTaskRunner : CidrTaskRunner {
    override suspend fun executeTask(
        project: Project,
        task: ProjectTask,
        sessionId: Any,
        context: ProjectTaskContext,
    ): Promise<ProjectTaskRunner.Result> {
        val configuration = task.cpBuildConfiguration()?.configuration
            ?: return resolvedPromise(TaskRunnerResults.FAILURE)
        val result = if (configuration.runCpBuild()) TaskRunnerResults.SUCCESS else TaskRunnerResults.FAILURE
        return resolvedPromise(result)
    }
}

private data class CpBuildConfiguration(val configuration: CpRunConfiguration) : CidrBuildConfiguration {
    override fun getName(): String = configuration.name

    override fun getExternalSource(): ProjectModelExternalSource? = null
}

private fun ProjectTask.cpBuildConfiguration(): CpBuildConfiguration? =
    (this as? ProjectModelBuildTask<*>)?.buildableElement as? CpBuildConfiguration

private class CpRunCommandLineState(environment: ExecutionEnvironment, private val configuration: CpRunConfiguration) :
    CommandLineState(environment) {
    override fun startProcess(): ProcessHandler {
        val source = configuration.mainFile.toPathOrNull()
            ?: throw ExecutionException("请选择 cp main 源文件")
        val compiler = CpRunPaths.resolveCompiler(configuration.project, configuration.compilerPath, source)
            ?: throw ExecutionException("找不到 cp 编译器")
        val executable = CpRunPaths.resolveBuildOutput(configuration.project, source)
        if (!Files.isRegularFile(executable)) {
            if (!configuration.runCpBuild()) {
                throw ExecutionException("cp 构建失败；请检查编译器路径、源文件和编译选项")
            }
        }
        if (!Files.isRegularFile(executable)) {
            throw ExecutionException("cp 构建没有生成可执行文件：$executable")
        }
        val stdlibRoot = CpRunPaths.resolveStdlibRoot(configuration.project, source, compiler)
        val workingDirectory = CpRunPaths.resolveWorkingDirectory(configuration.project, configuration.workingDirectory, source)
        val inputFile = resolveRedirectInputFileForExecution(configuration.redirectInput, configuration.redirectInputPath)
        val commandLine = buildCpRunCommandLine(
            executable = executable,
            programArguments = configuration.programArguments,
            workingDirectory = workingDirectory,
            envs = configuration.envs,
            passParentEnvs = configuration.passParentEnvs,
            inputFile = inputFile,
            emulateTerminal = configuration.emulateTerminal,
        )
        applyCpEnvironment(commandLine, compiler, stdlibRoot)
        return OSProcessHandler(commandLine)
    }
}

private data class CpBuildInvocation(val commandLine: GeneralCommandLine, val executable: Path)

private fun CpRunConfiguration.runCpBuild(): Boolean {
    val progress = startCpBuildProgress()
    return runCatching {
        val build = createBuildInvocation()
        progress.output("${build.commandLine.commandLineString}\n", ProcessOutputType.SYSTEM)
        Files.deleteIfExists(build.executable)
        val output = CapturingProcessHandler(build.commandLine).runProcess()
        progress.writeOutput(output.stdout, ProcessOutputType.STDOUT)
        progress.writeOutput(output.stderr, ProcessOutputType.STDERR)
        val success = output.exitCode == 0 && Files.isRegularFile(build.executable)
        if (success) {
            progress.finish(SuccessResultImpl())
            true
        } else {
            val message = "cp build failed with exit code ${output.exitCode}"
            CP_BUILD_LOG.warn("$message\n${output.stdout}\n${output.stderr}")
            progress.finish(FailureResultImpl(message))
            false
        }
    }.getOrElse {
        CP_BUILD_LOG.warn("cp build failed", it)
        val message = it.localizedMessage ?: it.javaClass.simpleName
        progress.output("$message\n", ProcessOutputType.STDERR)
        progress.finish(FailureResultImpl(message, it))
        false
    }
}

private fun CpRunConfiguration.startCpBuildProgress(): BuildProgress<BuildProgressDescriptor> {
    val descriptor = DefaultBuildDescriptor(
        Any(),
        "构建 '$name'",
        workingDirectory,
        System.currentTimeMillis(),
    )
    descriptor.setActivateToolWindowWhenAdded(true)
    descriptor.setActivateToolWindowWhenFailed(true)
    return BuildViewManager.createBuildProgress(project)
        .start(BuildProgressDescriptorImpl(descriptor))
}

private fun BuildProgress<BuildProgressDescriptor>.writeOutput(text: String, outputType: ProcessOutputType) {
    if (text.isNotBlank()) {
        output(text, outputType)
    }
}

private fun CpRunConfiguration.createBuildInvocation(): CpBuildInvocation {
    val source = mainFile.toPathOrNull()
        ?: throw ExecutionException("请选择 cp main 源文件")
    val compiler = CpRunPaths.resolveCompiler(project, compilerPath, source)
        ?: throw ExecutionException("找不到 cp 编译器")
    val executable = CpRunPaths.resolveBuildOutput(project, source)
    Files.createDirectories(executable.parent)
    val stdlibRoot = CpRunPaths.resolveStdlibRoot(project, source, compiler)
    val sources = CpRunPaths.resolveRunSources(project, source, compiler)
    val workingDirectory = CpRunPaths.resolveWorkingDirectory(project, workingDirectory, source)
    val commandLine = buildCpBuildCommandLine(
        compiler = compiler,
        sources = sources,
        compileOptions = compileOptions,
        executable = executable,
        workingDirectory = workingDirectory,
        envs = envs,
        passParentEnvs = passParentEnvs,
    )
    applyCpEnvironment(commandLine, compiler, stdlibRoot)
    return CpBuildInvocation(commandLine, executable)
}

private val CP_BUILD_LOG = Logger.getInstance(CpBuildBeforeRunTaskProvider::class.java)

internal fun buildCpBuildCommandLine(
    compiler: Path,
    sources: List<Path>,
    compileOptions: String,
    executable: Path,
    workingDirectory: Path,
    envs: Map<String, String> = emptyMap(),
    passParentEnvs: Boolean = true,
): GeneralCommandLine {
    val commandLine = GeneralCommandLine(compiler.toString())
        .withWorkDirectory(workingDirectory.toFile())
    commandLine.withParentEnvironmentType(parentEnvironmentType(passParentEnvs))
    commandLine.withEnvironment(envs)
    commandLine.addParameters(parseCpParameters(compileOptions))
    commandLine.addParameters(sources.map { it.toString() })
    commandLine.addParameters("-o", executable.toString())
    return commandLine
}

internal fun buildCpRunCommandLine(
    executable: Path,
    programArguments: String,
    workingDirectory: Path,
    envs: Map<String, String> = emptyMap(),
    passParentEnvs: Boolean = true,
    inputFile: File? = null,
    emulateTerminal: Boolean = false,
): GeneralCommandLine {
    val commandLine = if (emulateTerminal) {
        PtyCommandLine()
    } else {
        GeneralCommandLine()
    }
    commandLine.withExePath(executable.toString())
    commandLine.withWorkDirectory(workingDirectory.toFile())
    commandLine.withParentEnvironmentType(parentEnvironmentType(passParentEnvs))
    commandLine.withEnvironment(envs)
    commandLine.addParameters(parseCpParameters(programArguments))
    inputFile?.let { commandLine.withInput(it) }
    return commandLine
}

internal object CpRunPaths {
    fun resolveCompiler(project: Project, configured: String, source: Path): Path? =
        sequenceOf(
            configured.toPathOrNull(),
            System.getProperty("cp.compiler.path").toPathOrNull(),
            System.getenv("KCP").toPathOrNull(),
            pluginNativePath("cp"),
        )
            .plus(repoCompilerCandidates(project, source))
            .plus(
                sequenceOf(
                    findExecutableOnPath("kcp"),
                    System.getProperty("user.home")?.let { Path.of(it, ".local/bin/kcp") },
                ).filterNotNull(),
            )
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .firstOrNull { Files.isRegularFile(it) && Files.isExecutable(it) }

    fun resolveRunSources(project: Project, source: Path, compiler: Path): List<Path> =
        resolveSourceClosure(
            source = source,
            roots = importRoots(project, source, compiler),
            moduleSearchRoots = moduleSearchRoots(project, source, compiler),
        )

    fun resolveWorkingDirectory(project: Project, configured: String, source: Path): Path {
        configured.toPathOrNull()?.let {
            return it.toAbsolutePath().normalize()
        }
        project.basePath?.let {
            return Path.of(it).toAbsolutePath().normalize()
        }
        source.parent?.let {
            return it.toAbsolutePath().normalize()
        }
        return Path.of(".").toAbsolutePath().normalize()
    }

    fun resolveBuildOutput(project: Project, source: Path): Path =
        resolveBuildOutput(project.basePath?.let { Path.of(it) }, source)

    internal fun resolveBuildOutput(projectBase: Path?, source: Path): Path {
        val normalizedSource = source.toAbsolutePath().normalize()
        val projectKey = stableHash(projectBase?.toAbsolutePath()?.normalize()?.toString() ?: "default").take(16)
        val sourceKey = stableHash(normalizedSource.toString()).take(16)
        val executableName = normalizedSource.fileName.toString().removeSuffix(".${CpFileType.INSTANCE.defaultExtension}")
        return Path.of(PathManager.getSystemPath(), "cp-run", projectKey, sourceKey, executableName)
    }

    internal fun resolveSourceClosure(source: Path, roots: List<Path>, moduleSearchRoots: List<Path> = roots): List<Path> {
        val normalizedSource = source.toAbsolutePath().normalize()
        val knownFiles = sourceClosureKnownFiles(normalizedSource, roots, moduleSearchRoots)
        val request = CpInspectionResolveRequest(
            activeFile = normalizedSource.toString(),
            targetFile = normalizedSource.toString(),
            entryFiles = listOf(normalizedSource.toString()),
            importRoots = roots.map { it.toAbsolutePath().normalize().toString() },
            searchRoots = moduleSearchRoots.map { it.toAbsolutePath().normalize().toString() },
            followStdlibImports = false,
            files = knownFiles.values.toList(),
        )
        val resolved = CpResolvedRequestCache.resolve(request, knownFiles)
            ?: throw ExecutionException("cp 源文件依赖解析失败：$normalizedSource")
        if (resolved.files.isEmpty()) {
            throw ExecutionException("cp 源文件依赖解析结果为空：$normalizedSource")
        }
        return resolved.files.map { Path.of(it.path) }
    }

    fun resolveRuntimeLibrary(compiler: Path): Path? =
        listOf(
            System.getenv("CP_RUNTIME_LIBRARY_PATH").toPathOrNull(),
            compiler.parent?.resolve("libcp_runtime.a"),
            System.getProperty("user.home")?.let { Path.of(it, ".local/lib/kcp/libcp_runtime.a") },
            compiler.parent?.parent?.parent?.resolve("runtime/libcp_runtime.a"),
        )
            .filterNotNull()
            .firstOrNull { Files.isRegularFile(it) }

    fun resolveStdlibRoot(project: Project, source: Path, compiler: Path): Path? =
        stdlibRootCandidates(project, source, compiler)
            .firstOrNull { Files.isDirectory(it) }

    fun resolveStdlibRoot(compiler: Path): Path? =
        stdlibRootCandidates(project = null, source = null, compiler)
            .firstOrNull { Files.isDirectory(it) }

    private fun importRoots(project: Project, source: Path, compiler: Path): List<Path> =
        cpSourceImportRoots(
            source = source,
            projectBasePath = project.basePath,
            stdlibRoot = resolveStdlibRoot(project, source, compiler),
        )

    private fun moduleSearchRoots(project: Project, source: Path, compiler: Path): List<Path> =
        sequenceOf(
            source.parent,
            project.basePath?.let { Path.of(it) },
            resolveStdlibRoot(project, source, compiler),
        )
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .distinct()
            .toList()

    private fun stdlibRootCandidates(project: Project?, source: Path?, compiler: Path): Sequence<Path> =
        sequenceOf(System.getenv("CP_STDLIB_ROOT_PATH").toPathOrNull())
            .filterNotNull()
            .plus(project?.basePath?.let { sequenceOf(Path.of(it).resolve("std")) } ?: emptySequence())
            .plus(source?.parent.parentsFromSelf().map { it.resolve("std") })
            .plus(
                sequenceOf(
                    compiler.parent?.resolve("std"),
                    System.getProperty("user.home")?.let { Path.of(it, ".local/share/kcp/std") },
                    compiler.parent?.parent?.parent?.resolve("std"),
                ).filterNotNull(),
            )
            .map { it.toAbsolutePath().normalize() }
            .distinct()

    private fun findExecutableOnPath(name: String): Path? =
        System.getenv("PATH")
            ?.splitToSequence(java.io.File.pathSeparator)
            ?.map { Path.of(it).resolve(name) }
            ?.firstOrNull { Files.isRegularFile(it) && Files.isExecutable(it) }

    private fun repoCompilerCandidates(project: Project, source: Path): Sequence<Path> =
        listOfNotNull(project.basePath?.let { Path.of(it) }, source.parent)
            .asSequence()
            .flatMap { directory ->
                directory.parentsFromSelf()
                    .flatMap { root ->
                        sequenceOf(
                            root.resolve("build-clion-plugin-native/compiler/cp"),
                            root.resolve("build/compiler/cp"),
                            root.resolve("cmake-build-debug/compiler/cp"),
                        )
                    }
            }

    private fun pluginNativePath(name: String): Path? {
        val classLocation = runCatching {
            Path.of(CpRunPaths::class.java.protectionDomain.codeSource.location.toURI())
        }.getOrNull() ?: return null
        val pluginRoot = when {
            Files.isRegularFile(classLocation) -> classLocation.parent?.parent
            else -> classLocation
        } ?: return null
        return pluginRoot.resolve("native/linux-x86_64").resolve(name)
    }

    private fun sourceClosureKnownFiles(source: Path, roots: List<Path>, moduleSearchRoots: List<Path>): Map<String, CpInspectionFile> {
        val files = linkedMapOf<String, CpInspectionFile>()
        val rootSet = (roots + moduleSearchRoots + listOfNotNull(source.parent))
            .map { it.toAbsolutePath().normalize() }
            .distinct()
        for (root in rootSet) {
            collectCpFiles(root, files)
        }
        if (Files.isRegularFile(source)) {
            files[source.toString()] = CpInspectionFile(
                path = source.toString(),
                text = Files.readString(source),
            )
        }
        return files
    }

    private fun collectCpFiles(root: Path, files: MutableMap<String, CpInspectionFile>) {
        if (!Files.exists(root)) {
            return
        }
        if (Files.isRegularFile(root)) {
            collectCpFile(root, files)
            return
        }
        if (!Files.isDirectory(root)) {
            return
        }
        val sourcePaths = mutableListOf<Path>()
        try {
            Files.walkFileTree(
                root,
                object : SimpleFileVisitor<Path>() {
                    override fun visitFile(file: Path, attrs: BasicFileAttributes): FileVisitResult {
                        if (attrs.isRegularFile && file.fileName.toString().endsWith(".${CpFileType.INSTANCE.defaultExtension}")) {
                            sourcePaths.add(file.toAbsolutePath().normalize())
                        }
                        return FileVisitResult.CONTINUE
                    }

                    override fun visitFileFailed(file: Path, exc: IOException): FileVisitResult =
                        FileVisitResult.CONTINUE
                },
            )
        } catch (_: IOException) {
            return
        }
        sourcePaths.sorted().forEach { collectCpFile(it, files) }
    }

    private fun collectCpFile(path: Path, files: MutableMap<String, CpInspectionFile>) {
        if (!path.fileName.toString().endsWith(".${CpFileType.INSTANCE.defaultExtension}")) {
            return
        }
        val normalized = path.toAbsolutePath().normalize().toString()
        val text = runCatching { Files.readString(path) }.getOrNull() ?: return
        files[normalized] = CpInspectionFile(
            path = normalized,
            text = text,
        )
    }
}

private fun applyCpEnvironment(commandLine: GeneralCommandLine, compiler: Path, stdlibRoot: Path?) {
    CpRunPaths.resolveRuntimeLibrary(compiler)?.let {
        commandLine.withEnvironment("CP_RUNTIME_LIBRARY_PATH", it.toString())
    }
    stdlibRoot?.let {
        commandLine.withEnvironment("CP_STDLIB_ROOT_PATH", it.toString())
    }
}

private fun validateCpParameters(label: String, value: String) {
    runCatching { parseCpParameters(value) }.getOrElse {
        throw RuntimeConfigurationError("$label 无法解析：${it.message ?: it.javaClass.simpleName}")
    }
}

private fun parentEnvironmentType(passParentEnvs: Boolean): GeneralCommandLine.ParentEnvironmentType =
    if (passParentEnvs) {
        GeneralCommandLine.ParentEnvironmentType.CONSOLE
    } else {
        GeneralCommandLine.ParentEnvironmentType.NONE
    }

private fun parseCpParameters(value: String): List<String> =
    ParametersListUtil.parse(value)

internal fun resolveRedirectInputFile(redirectInput: Boolean, redirectInputPath: String): File? {
    if (!redirectInput) {
        return null
    }
    val input = redirectInputPath.toPathOrNull()
        ?: throw RuntimeConfigurationError("重定向输入文件不能为空")
    if (!Files.isRegularFile(input)) {
        throw RuntimeConfigurationError("重定向输入文件不存在：$input")
    }
    return input.toFile()
}

private fun resolveRedirectInputFileForExecution(redirectInput: Boolean, redirectInputPath: String): File? =
    try {
        resolveRedirectInputFile(redirectInput, redirectInputPath)
    } catch (error: RuntimeConfigurationError) {
        throw ExecutionException(error.localizedMessage, error)
    }

private fun stableHash(value: String): String =
    HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value.toByteArray(Charsets.UTF_8)))

private fun PsiElement.cpTopLevelMainFunction(): PsiElement? {
    val function = if (cpElementType() == CpElements.FUNCTION) {
        this
    } else {
        parentByType(CpElements.FUNCTION)
    } ?: return null
    val name = function.directChild(CpElements.FUNCTION_NAME) ?: return null
    if (name.text != "main") {
        return null
    }
    if (function.parentByType(CpElements.IMPL_BLOCK) != null || function.parentByType(CpElements.CONCEPT_DECLARATION) != null) {
        return null
    }
    return function
}

private fun String?.toPathOrNull(): Path? =
    takeUnless { it.isNullOrBlank() }?.let { Path.of(it) }
