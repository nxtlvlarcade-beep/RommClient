pipeline {
    agent { label 'libromm' }
    options { timestamps(); disableConcurrentBuilds() }
    stages {
        stage('Build') { steps { sh 'set -eux; make clean; make' } }
        stage('Verify') { steps { sh 'test -f libromm.a; test -f libromm-curl.a; test -x romm-cli; file libromm.a libromm-curl.a romm-cli' } }
    }
    post { success { archiveArtifacts artifacts: 'libromm.a,libromm-curl.a,romm-cli,include/*.h', fingerprint: true } }
}
