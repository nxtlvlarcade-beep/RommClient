pipeline {
    agent any

    options {
        timestamps()
        disableConcurrentBuilds()
    }

    stages {
        stage('Build') {
            steps {
                sh '''
                    set -eux

                    echo "=== Build environment ==="
                    uname -a
                    cc --version
                    make --version

                    echo "=== Clean ==="
                    make clean || true

                    echo "=== Build libromm ==="
                    make
                '''
            }
        }

        stage('Verify') {
            steps {
                sh '''
                    set -eux

                    test -x romm-cli
                    file romm-cli

                    echo "libromm build successful."
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

        always {
            sh 'make clean || true'
        }
    }
}
