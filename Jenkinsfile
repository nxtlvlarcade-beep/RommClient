pipeline {
    agent { label 'dev-v1.0' }
    options {
        timestamps()
        disableConcurrentBuilds()
        skipDefaultCheckout(true)
        buildDiscarder(logRotator(numToKeepStr: '20', artifactNumToKeepStr: '10'))
    }

    parameters {
        booleanParam(name: 'REQUIRE_AMIGA', defaultValue: false,
                     description: 'Build als Fehler markieren, wenn der m68k-AmigaOS-Compiler fehlt')
        booleanParam(name: 'BUILD_DOCKER', defaultValue: false,
                     description: 'Docker-Gateway-Image bauen (Docker auf dem Jenkins-Agent erforderlich)')
    }

    environment {
        DOCKER_IMAGE = 'romm-retro-gateway'
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
                sh 'git rev-parse --short HEAD && git status --short'
            }
        }

        stage('Quellcode pruefen') {
            steps {
                sh '''#!/bin/sh
                    set -eu
                    test -f Makefile
                    test -f Makefile.amiga
                    test -f extras/romm-retro-gateway/Dockerfile
                    test -f extras/romm-retro-gateway/Caddyfile
                    for f in extras/romm-retro-gateway/*.py; do
                        if command -v python3 >/dev/null 2>&1; then
                            python3 - "$f" <<'PY'
import pathlib, sys
compile(pathlib.Path(sys.argv[1]).read_bytes(), sys.argv[1], 'exec')
PY
                        else
                            echo 'Python3 fehlt: Python-Syntaxpruefung uebersprungen'
                            break
                        fi
                    done
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
                '''
            }
        }

        stage('AmigaOS m68k') {
            steps {
                script {
                    if (sh(script: 'command -v m68k-amigaos-gcc >/dev/null 2>&1 && command -v m68k-amigaos-ar >/dev/null 2>&1', returnStatus: true) == 0) {
                        sh '''#!/bin/sh
                            set -eu
                            make -f Makefile.amiga clean
                            make -f Makefile.amiga -j2 all
                            test -s romm-amiga
                        '''
                    } else if (params.REQUIRE_AMIGA) {
                        error('m68k-amigaos-gcc / m68k-amigaos-ar fehlen auf diesem Jenkins-Agent.')
                    } else {
                        echo 'AmigaOS-Build uebersprungen: m68k-AmigaOS-Toolchain nicht installiert.'
                    }
                }
            }
        }

        stage('Docker-Gateway') {
            when { expression { return params.BUILD_DOCKER } }
            steps {
                sh '''#!/bin/sh
                    set -eu
                    command -v docker >/dev/null 2>&1 || {
                        echo 'Docker ist auf dem Jenkins-Agent nicht installiert.' >&2
                        exit 1
                    }
                    docker info >/dev/null
                    docker build \
                        -t "${DOCKER_IMAGE}:${BUILD_NUMBER}" \
                        -f extras/romm-retro-gateway/Dockerfile .
                '''
            }
        }
    }

    post {
        always {
            archiveArtifacts artifacts: 'libromm.a,libromm-curl.a,romm-cli,romm-tui,romm-amiga',
                             allowEmptyArchive: true, fingerprint: true
        }
        success {
            echo 'RommClient CI abgeschlossen.'
        }
        failure {
            echo 'Build fehlgeschlagen: fehlende Abhaengigkeiten oder Compilerfehler im Stage-Log pruefen.'
        }
    }
}
