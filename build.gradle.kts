plugins {
    id("java")
    application
}

group = "org.retr0a"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    implementation("net.jthink:jaudiotagger:3.0.1")
    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

application {
    mainClass.set("org.retr0a.tagtune.Main")
    applicationDefaultJvmArgs = listOf("-Xdock:name=TagTune")
}

tasks.register<Exec>("createIcns") {
    val iconPng = file("src/main/resources/Icon-iOS-Default-1024x1024@1x.png")
    val iconsetDir = layout.buildDirectory.dir("TagTune.iconset")
    val icnsFile = layout.buildDirectory.file("TagTune.icns")

    inputs.file(iconPng)
    outputs.file(icnsFile)

    commandLine("sh", "-c", """
        mkdir -p "${iconsetDir.get().asFile.absolutePath}"
        for size in 16 32 128 256 512; do
            sips -z ${'$'}size ${'$'}size "${iconPng.absolutePath}" --out "${iconsetDir.get().asFile.absolutePath}/icon_${'$'}{size}x${'$'}{size}.png"
            size2=$((size * 2))
            sips -z ${'$'}size2 ${'$'}size2 "${iconPng.absolutePath}" --out "${iconsetDir.get().asFile.absolutePath}/icon_${'$'}{size}x${'$'}{size}@2x.png"
        done
        iconutil -c icns "${iconsetDir.get().asFile.absolutePath}" -o "${icnsFile.get().asFile.absolutePath}"
    """.trimIndent())
}

tasks.register<Exec>("packageDmg") {
    dependsOn("build", "createIcns")

    val libsDir = layout.buildDirectory.dir("libs")
    val icnsFile = layout.buildDirectory.file("TagTune.icns")
    val buildDir = layout.buildDirectory

    doFirst {
        val jarFile = libsDir.get().asFile
            .listFiles()
            ?.firstOrNull { it.name.endsWith(".jar") && !it.name.contains("plain") }
            ?: throw RuntimeException("Jar not found in ${libsDir.get().asFile.absolutePath}")

        commandLine(
            "jpackage",
            "--input", libsDir.get().asFile.absolutePath,
            "--name", "TagTune",
            "--main-jar", jarFile.name,
            "--main-class", "org.retr0a.tagtune.Main",
            "--type", "dmg",
            "--icon", icnsFile.get().asFile.absolutePath,
            "--java-options", "-Xdock:name=TagTune",
            "--dest", buildDir.get().asFile.absolutePath
        )
    }
}

tasks.test {
    useJUnitPlatform()
}
