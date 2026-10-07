pipeline {
    agent {
        label 'libromm'
    }

    options {
        timestamps()
        disableConcurrentBuilds()
    }

    stages {
        stage('Environment') {
            steps {
                sh '''
                    uname -a
                    gcc --version
                    make --version
                    curl --version
                    pkg-config --modversion libcurl
                '''
            }
        }

        stage('Build') {
            steps {
                sh '''
                    set -eux
                    make clean || true
                    make
                '''
            }
        }

        stage('Verify') {
            steps {
                sh '''
                    test -x romm-cli
                    file romm-cli
                    ldd romm-cli || true
                '''
            }
        }
    }

    post {
        success {
            archiveArtifacts(
                artifacts: 'romm-cli',
                fingerprint: true
            )
        }
    }
}
