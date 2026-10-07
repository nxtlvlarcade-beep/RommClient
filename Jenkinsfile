pipeline {
    agent {
        label 'libromm'
    }

    environment {
        PATH = "/opt/amiga/bin:${env.PATH}"
    }

    stages {
        stage('Toolchains') {
            steps {
                sh '''
                    set -eux

                    gcc --version
                    make --version

                    which m68k-amigaos-gcc
                    m68k-amigaos-gcc --version

                    which m68k-amigaos-ar
                    which m68k-amigaos-objdump
                '''
            }
        }

        stage('Clean') {
            steps {
                sh '''
                    set -eux

                    make clean || true
                    make -f Makefile.amiga clean || true
                '''
            }
        }

        stage('Build Linux') {
            steps {
                sh '''
                    set -eux

                    make all

                    test -f libromm.a
                    test -f libromm-curl.a
                    test -x romm-cli
                    test -x romm-tui

                    file romm-cli
                    file romm-tui
                '''
            }
        }

        stage('Build AmigaOS 68k') {
            steps {
                sh '''
                    set -eux

                    make -f Makefile.amiga all

                    test -f romm-amiga

                    file romm-amiga
                    m68k-amigaos-objdump -f romm-amiga
                '''
            }
        }

        stage('Artifact Info') {
            steps {
                sh '''
                    set -eux

                    echo "=== Linux ==="
                    ls -lh \
                        libromm.a \
                        libromm-curl.a \
                        romm-cli \
                        romm-tui

                    echo "=== AmigaOS ==="
                    ls -lh romm-amiga

                    echo "=== Headers ==="
                    ls -lh include/
                '''
            }
        }
    }

    post {
        success {
            archiveArtifacts(
                artifacts: 'libromm.a,libromm-curl.a,romm-cli,romm-tui,romm-amiga,include/*.h',
                fingerprint: true
            )
        }

        always {
            echo 'libromm build finished'
        }
    }
}
