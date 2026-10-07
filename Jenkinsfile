pipeline {
    agent { label 'libromm' }
    options { timestamps(); disableConcurrentBuilds() }
    stages {
        stage('Build') { steps { sh 'set -eux; make clean; make' } }
        stage('Verify') {
    steps {
        sh '''
            test -f libromm.a
            test -f libromm-curl.a
            test -x romm-cli
            test -x romm-tui
            file libromm.a libromm-curl.a romm-cli romm-tui
        '''
    }
}

post {
    success {
        archiveArtifacts artifacts: 'libromm.a,libromm-curl.a,romm-cli,romm-tui,include/*.h',
                         fingerprint: true
    }
}
