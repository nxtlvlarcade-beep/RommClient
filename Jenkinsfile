pipeline {
    agent { label 'libromm' }

    options {
        timestamps()
        disableConcurrentBuilds()
    }

    environment {
        // AmigaOS m68k cross toolchain
        PATH = "/opt/amiga/bin:${env.PATH}"
    }

    stages {
        stage('Toolchains') {
            steps {
                sh '''
                    set -eux

                    echo "=== Linux toolchain ==="
                    gcc --version
                    make --version

                    echo "=== AmigaOS toolchain ==="
                    which m68k-amigaos-gcc
                    which m68k-amigaos-ar
                    which m68k-amigaos-objdump

                    m68k-amigaos-gcc --version
                    m68k-amigaos-ar --version
                '''
            }
        }

        stage('Build Linux') {
            steps {
                sh '''
                    set -eux

                    make clean
                    make
                '''
            }
        }

        stage('Verify Linux') {
            steps {
                sh '''
                    set -eux

                    test -f libromm.a
                    test -f libromm-curl.a
                    test -x romm-cli
                    test -x romm-tui

                    echo "=== Linux artifacts ==="

                    file \
                        libromm.a \
                        libromm-curl.a \
                        romm-cli \
                        romm-tui
                '''
            }
        }

        stage('Build AmigaOS 68k') {
            steps {
                sh '''
                    set -eux

                    make -f Makefile.amiga clean
                    make -f Makefile.amiga
                '''
            }
        }

        stage('Verify AmigaOS 68k') {
            steps {
                sh '''
                    set -eux

                    test -f romm-amiga

                    echo "=== AmigaOS artifact ==="

                    file romm-amiga

                    echo "=== AmigaOS executable information ==="

                    m68k-amigaos-objdump -f romm-amiga
                '''
            }
        }

        stage('Artifact Info') {
            steps {
                sh '''
                    set -eux

                    echo "=== File sizes ==="

                    ls -lh \
                        libromm.a \
                        libromm-curl.a \
                        romm-cli \
                        romm-tui \
                        romm-amiga
                '''
            }
        }
    }

    post {
        success {
            archiveArtifacts(
                artifacts: '''
                    libromm.a,
                    libromm-curl.a,
                    romm-cli,
                    romm-tui,
                    romm-amiga,
                    include/*.h
                '''.replaceAll('\\s+', ''),
                fingerprint: true
            )
        }

        always {
            echo 'libromm Linux + AmigaOS build finished.'
        }
    }
}
