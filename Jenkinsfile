pipeline {
    agent { label 'dev-v1.0' }

    options {
        timestamps()
        disableConcurrentBuilds()
        skipDefaultCheckout(true)
        buildDiscarder(logRotator(
            numToKeepStr: '20',
            artifactNumToKeepStr: '10'
        ))
    }

    parameters {
        booleanParam(
            name: 'REQUIRE_AMIGA',
            defaultValue: true,
            description: 'Build als Fehler markieren, wenn die m68k-AmigaOS-Toolchain fehlt'
        )
        booleanParam(
            name: 'BUILD_DOCKER',
            defaultValue: false,
            description: 'Docker-Gateway-Image bauen (Docker auf dem Jenkins-Agent erforderlich)'
        )
    }

    environment {
        DOCKER_IMAGE = 'romm-retro-gateway'
    }

    stages {
        stage('Checkout') {
            steps {
                deleteDir()
                checkout scm

                sh '''#!/bin/sh
                    set -eu
                    echo "=== Git-Revision ==="
                    git rev-parse --short HEAD
                    git status --short

                    echo "=== Jenkins-Agent ==="
                    hostname
                    echo "WORKSPACE=$WORKSPACE"
                '''
            }
        }

        stage('Quellcode pruefen') {
            steps {
                sh '''#!/bin/sh
                    set -eu

                    test -f Makefile
                    test -f Makefile.amiga
                    test -f Jenkinsfile
                    test -f examples/romm_amiga.c
                    test -f examples/romm_whdload.c
                    test -f extras/romm-retro-gateway/Dockerfile
                    test -f extras/romm-retro-gateway/Caddyfile

                    command -v make
                    command -v python3

                    for f in extras/romm-retro-gateway/*.py; do
                        python3 - "$f" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
compile(path.read_bytes(), str(path), 'exec')
print("Python OK:", path)
PY
                    done

                    echo "Quellcodepruefung erfolgreich."
                '''
            }
        }

        stage('Linux: Bibliotheken und Clients') {
            steps {
                sh '''#!/bin/sh
                    set -eu

                    make clean
                    make -j2 all

                    test -s libromm.a
                    test -s libromm-curl.a
                    test -x romm-cli
                    test -x romm-tui

                    echo "=== Linux-Artefakte ==="
                    ls -lh \
                        libromm.a \
                        libromm-curl.a \
                        romm-cli \
                        romm-tui
                '''
            }
        }

        stage('AmigaOS m68k: Beide Binaries') {
            steps {
                script {
                    int toolchainStatus = sh(
                        script: '''#!/bin/sh
                            command -v m68k-amigaos-gcc >/dev/null 2>&1 &&
                            command -v m68k-amigaos-ar >/dev/null 2>&1
                        ''',
                        returnStatus: true
                    )

                    if (toolchainStatus == 0) {
                        sh '''#!/bin/sh
                            set -eu

                            echo "=== AmigaOS-Toolchain ==="
                            command -v m68k-amigaos-gcc
                            command -v m68k-amigaos-ar

                            echo "=== AmigaOS Clean ==="
                            make -f Makefile.amiga clean

                            echo "=== Beide AmigaOS-Binaries bauen ==="
                            make -f Makefile.amiga -j2 all

                            echo "=== Binaries pruefen ==="
                            test -s romm-amiga
                            test -s romm-whdload

                            echo "=== AmigaOS-Artefakte ==="
                            ls -lh romm-amiga romm-whdload
                            file romm-amiga romm-whdload

                            echo "Beide AmigaOS-Binaries erfolgreich erstellt."
                        '''
                    } else if (params.REQUIRE_AMIGA) {
                        error(
                            'AmigaOS-Build nicht moeglich: ' +
                            'm68k-amigaos-gcc oder m68k-amigaos-ar fehlt.'
                        )
                    } else {
                        echo(
                            'AmigaOS-Build uebersprungen: ' +
                            'm68k-AmigaOS-Toolchain nicht installiert.'
                        )
                    }
                }
            }
        }

        stage('Docker-Gateway') {
            when {
                expression { return params.BUILD_DOCKER }
            }

            steps {
                sh '''#!/bin/sh
                    set -eu

                    command -v docker >/dev/null 2>&1 || {
                        echo "Docker ist auf dem Jenkins-Agent nicht installiert." >&2
                        exit 1
                    }

                    docker info >/dev/null

                    docker build \
                        -t "${DOCKER_IMAGE}:${BUILD_NUMBER}" \
                        -f extras/romm-retro-gateway/Dockerfile \
                        .
                '''
            }
        }
    }

    post {
        always {
            archiveArtifacts(
                artifacts: 'libromm.a,libromm-curl.a,romm-cli,romm-tui,romm-amiga,romm-whdload',
                allowEmptyArchive: true,
                fingerprint: true
            )
        }

        success {
            echo 'RommClient CI erfolgreich: Linux- und angeforderte AmigaOS-Builds abgeschlossen.'
        }

        failure {
            echo 'Build fehlgeschlagen. Bitte die fehlgeschlagene Stage im Log pruefen.'
        }
    }
}

