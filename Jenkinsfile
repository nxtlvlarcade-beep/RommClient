pipeline {
    agent { label 'libromm' }

    options {
        timestamps()
        disableConcurrentBuilds()
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
                    make all
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
                    file libromm.a libromm-curl.a romm-cli romm-tui
                '''
            }
        }

        stage('Build AmigaOS 68k') {
            steps {
                sh '''
                    set -eux
                    make -f Makefile.amiga clean
                    make -f Makefile.amiga all
                '''
            }
        }

        stage('Verify AmigaOS 68k') {
            steps {
                sh '''
                    set -eux
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
                    ls -lh libromm.a libromm-curl.a romm-cli romm-tui romm-amiga include/*.h
                '''
            }
        }
    }

    post {
        success {
            archiveArtifacts artifacts: 'libromm.a,libromm-curl.a,romm-cli,romm-tui,romm-amiga,include/*.h', fingerprint: true
        }
    }
}
