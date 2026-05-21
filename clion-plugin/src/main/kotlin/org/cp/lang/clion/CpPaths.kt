package org.cp.lang.clion

import java.nio.file.Path

internal fun cpNormalizePath(path: String): String =
    runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)

internal fun Path?.parentsFromSelf(): Sequence<Path> =
    generateSequence(this?.toAbsolutePath()?.normalize()) { it.parent }
