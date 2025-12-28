# CI/CD Documentation

## Overview

This repository uses GitHub Actions for continuous integration.

## Workflows

### 1. **CI** (`.github/workflows/ci.yml`) ⭐ Main Workflow

**Runs automatically on:**
- Pull request creation/updates
- Push to `main` or `develop` branches

**What it does:**
- ✅ Builds on Ubuntu 22.04 & 20.04
- ✅ Tests both Release & Debug builds
- ✅ Runs unit tests (no GCP auth required)
- ✅ Code quality checks (clang-format, cppcheck)
- ✅ Uploads build artifacts

**No setup required!** This workflow runs automatically without any configuration.

### 2. **E2E Tests (Manual)** (`.github/workflows/e2e-tests.yml`) 

**Runs manually only** via GitHub UI or CLI.

**What it does:**
- Builds gcscfuse
- Authenticates to GCP
- Runs E2E tests against real GCS bucket

**How to run:**

1. **Via GitHub UI:**
   - Go to Actions → E2E Tests (Manual) → Run workflow
   - Enter your test bucket name
   - Click "Run workflow"

2. **Via GitHub CLI:**
   ```bash
   gh workflow run e2e-tests.yml -f test_bucket=your-test-bucket
   ```

**Requirements:**
- `GCP_SERVICE_ACCOUNT_KEY` secret (see setup below)
- Test bucket in GCS

## Quick Start

### For Regular Development (No Setup Needed!)

Just create PRs normally. The CI workflow will:
1. Build your code on multiple platforms
2. Run unit tests
3. Report status ✅/❌

### For E2E Testing (Optional)

<details>
<summary>Click to expand E2E setup instructions</summary>

#### 1. Create GCP Service Account

```bash
export PROJECT_ID="your-project-id"

gcloud iam service-accounts create gcscfuse-ci \
    --display-name="gcscfuse CI Testing"

gcloud projects add-iam-policy-binding $PROJECT_ID \
    --member="serviceAccount:gcscfuse-ci@${PROJECT_ID}.iam.gserviceaccount.com" \
    --role="roles/storage.objectAdmin"

gcloud iam service-accounts keys create key.json \
    --iam-account="gcscfuse-ci@${PROJECT_ID}.iam.gserviceaccount.com"
```

#### 2. Create Test Bucket

```bash
gsutil mb gs://gcscfuse-ci-test-${PROJECT_ID}
```

#### 3. Configure GitHub Secret

```bash
gh secret set GCP_SERVICE_ACCOUNT_KEY < key.json
rm key.json  # Delete local copy
```

#### 4. Run E2E Tests

```bash
# Via GitHub UI
Go to: Actions → E2E Tests (Manual) → Run workflow

# Via CLI
gh workflow run e2e-tests.yml -f test_bucket=gcscfuse-ci-test-${PROJECT_ID}
```

</details>

## Testing Locally

```bash
# Build and run unit tests (no GCP needed)
./.github/validate-local.sh

# Or manually:
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
ctest --output-on-failure
```

## Workflow Status

Check workflow runs:
```bash
gh run list
gh run view <run-id>
```

## Files

```
.github/
├── workflows/
│   ├── ci.yml           # Main CI (auto, no setup)
│   └── e2e-tests.yml    # E2E tests (manual, needs GCP)
├── validate-local.sh    # Local testing script
└── README.md            # This file
```

## FAQ

**Q: Do I need to configure anything to use CI?**  
A: No! The main CI workflow runs automatically on PRs without any setup.

**Q: How do I run E2E tests?**  
A: E2E tests are optional and manual-only. They require GCP credentials (see "E2E Testing" section above).

**Q: Why are builds slow the first time?**  
A: First build installs dependencies (~60 min). Subsequent builds use cache (~5-10 min).

**Q: Can I run tests locally?**  
A: Yes! Use `./.github/validate-local.sh` or build manually with cmake/make.

**Q: How do I see test results?**  
A: Check the Actions tab in GitHub, or the PR status checks.

## Support

For issues or questions:
- Check workflow logs in Actions tab
- Review [validate-local.sh](validate-local.sh) for local testing
- Ensure vcpkg dependencies are installed
