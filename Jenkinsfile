/*
 * Copyright (c) 2026 IBM Corporation. All rights reserved.
 *
 * Jenkins Declarative Pipeline for lsvpd
 * Translates Travis CI workflow to Jenkins with matrix builds
 * Supports: Fedora 37, Ubuntu 24.04
 * Container Runtime: Docker/Podman (auto-detected)
 */

pipeline {
    agent {
        label 'ras_ci'
    }

    triggers {
        // Trigger on push to any branch
        githubPush()
    }

    environment {
        // Git commit information
        GIT_COMMIT_MSG = ''

        // Container runtime (auto-detected)
        CONTAINER_CMD = ''

        // PR information
        PR_NUMBER = "${env.CHANGE_ID ?: 'N/A'}"
        PR_TITLE = "${env.CHANGE_TITLE ?: 'N/A'}"
        PR_AUTHOR = "${env.CHANGE_AUTHOR ?: 'N/A'}"
        TARGET_BRANCH = "${env.CHANGE_TARGET ?: env.BRANCH_NAME}"

        // Build configuration
        CCACHE_DIR = "${HOME}/.ccache"
        WORKSPACE_DIR = "${WORKSPACE}"
    }

    stages {
        stage('Init') {
            steps {
                script {
                    echo "=========================================="
                    echo "Initializing lsvpd CI/CD Pipeline"
                    echo "=========================================="

                    // Display build type
                    if (env.CHANGE_ID) {
                        echo "Build Type: Pull Request"
                        echo "PR Number: ${PR_NUMBER}"
                        echo "PR Title: ${PR_TITLE}"
                        echo "PR Author: ${PR_AUTHOR}"
                        echo "Target Branch: ${TARGET_BRANCH}"
                    } else {
                        echo "Build Type: Push to ${env.BRANCH_NAME}"
                    }

                    // Get commit message
                    GIT_COMMIT_MSG = sh(
                        script: 'git log -1 --pretty=format:"%s" ${GIT_COMMIT}',
                        returnStdout: true
                    ).trim()
                    echo "Commit: ${GIT_COMMIT_MSG}"

                    // Detect container runtime
                    CONTAINER_CMD = sh(
                        script: '''
                            if command -v docker &> /dev/null; then
                                echo "docker"
                            elif command -v podman &> /dev/null; then
                                echo "podman"
                            else
                                echo "none"
                            fi
                        ''',
                        returnStdout: true
                    ).trim()

                    if (CONTAINER_CMD == "none") {
                        error "Neither Docker nor Podman is available on this node"
                    }

                    echo "Container Runtime: ${CONTAINER_CMD}"
                }
            }
        }

        stage('Build Matrix') {
            matrix {
                axes {
                    axis {
                        name 'DISTRO'
                        values 'fedora-37', 'ubuntu-24.04'
                    }
                }

                stages {
                    stage('Build Image - ${DISTRO}') {
                        steps {
                            script {
                                echo "=========================================="
                                echo "Building ${DISTRO} container image"
                                echo "=========================================="

                                sh """
                                    ${CONTAINER_CMD} build --pull \
                                        -v /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem:/tmp/host-ca-bundle.crt:ro \
                                        -t lsvpd-${DISTRO} \
                                        -f lsvpd-ci/Dockerfile-${DISTRO} \
                                        .
                                """
                            }
                        }
                    }

                    stage('Run Build - ${DISTRO}') {
                        steps {
                            script {
                                echo "=========================================="
                                echo "Running ${DISTRO} build"
                                echo "=========================================="

                                // Create ccache directory if it doesn't exist
                                sh "mkdir -p ${CCACHE_DIR}"

                                // Run the build container
                                sh """
                                    ${CONTAINER_CMD} run \
                                        --security-opt seccomp=unconfined \
                                        --volume ${CCACHE_DIR}:/root/.ccache \
                                        --volume ${WORKSPACE_DIR}:/build:z \
                                        --tmpfs /tmp/ \
                                        --rm -t lsvpd-${DISTRO} \
                                        bash -o pipefail -c "./lsvpd-ci/build-${DISTRO}.sh" 2>&1 | tee build-${DISTRO}.log
                                """
                            }
                        }
                    }

                    stage('Archive Artifacts - ${DISTRO}') {
                        steps {
                            script {
                                echo "=========================================="
                                echo "Archiving ${DISTRO} artifacts"
                                echo "=========================================="

                                // Archive build logs
                                archiveArtifacts(
                                    artifacts: "build-${DISTRO}.log",
                                    allowEmptyArchive: true
                                )

                                // Archive distribution-specific artifacts
                                if (DISTRO == 'fedora-37') {
                                    // Archive RPM packages
                                    sh """
                                        if [ -d ~/rpmbuild/RPMS ]; then
                                            mkdir -p artifacts/${DISTRO}
                                            find ~/rpmbuild/RPMS -name '*.rpm' -exec cp {} artifacts/${DISTRO}/ \\; || true
                                        fi
                                    """

                                    archiveArtifacts(
                                        artifacts: "artifacts/${DISTRO}/*.rpm",
                                        allowEmptyArchive: true
                                    )
                                }

                                // Archive tarballs (both distributions)
                                sh """
                                    mkdir -p artifacts/${DISTRO}
                                    find . -maxdepth 1 -name '*.tar.gz' -exec cp {} artifacts/${DISTRO}/ \\; || true
                                """

                                archiveArtifacts(
                                    artifacts: "artifacts/${DISTRO}/*.tar.gz",
                                    allowEmptyArchive: true
                                )
                            }
                        }
                    }

                    stage('Cleanup - ${DISTRO}') {
                        steps {
                            script {
                                echo "Cleaning up ${DISTRO} container image"
                                sh "${CONTAINER_CMD} rmi lsvpd-${DISTRO} || true"
                            }
                        }
                    }
                }
            }
        }
    }

    post {
        success {
            echo "=========================================="
            echo "Pipeline completed successfully!"
            echo "=========================================="
            echo "All distributions built successfully"
            echo "Artifacts archived for:"
            echo "  - Fedora 37"
            echo "  - Ubuntu 24.04"
        }

        failure {
            echo "=========================================="
            echo "Pipeline failed!"
            echo "=========================================="
            echo "Check build logs for details"
            echo "Commit: ${GIT_COMMIT_MSG}"
        }

        unstable {
            echo "=========================================="
            echo "Pipeline unstable"
            echo "=========================================="
            echo "Some builds may have warnings"
        }

        always {
            script {
                echo "=========================================="
                echo "Cleaning up workspace"
                echo "=========================================="

                // Clean up any remaining containers
                sh """
                    ${CONTAINER_CMD} ps -a | grep lsvpd- | awk '{print \$1}' | xargs -r ${CONTAINER_CMD} rm -f || true
                """

                // Clean up dangling images
                sh """
                    ${CONTAINER_CMD} images -f dangling=true -q | xargs -r ${CONTAINER_CMD} rmi || true
                """

                // Archive workspace info
                sh """
                    echo "Build Number: ${BUILD_NUMBER}" > build-info.txt
                    echo "Build URL: ${BUILD_URL}" >> build-info.txt
                    echo "Git Commit: ${GIT_COMMIT}" >> build-info.txt
                    echo "Git Commit Message: ${GIT_COMMIT_MSG}" >> build-info.txt
                    echo "Container Runtime: ${CONTAINER_CMD}" >> build-info.txt
                    echo "Build Date: \$(date)" >> build-info.txt
                """

                archiveArtifacts(
                    artifacts: 'build-info.txt',
                    allowEmptyArchive: true
                )

                // Clean workspace (optional - uncomment if needed)
                // cleanWs()
            }
        }
    }
}
