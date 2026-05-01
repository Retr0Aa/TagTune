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

tasks.test {
    useJUnitPlatform()
}
