# CI/CD Setup for gcscfuse

This repository uses GitHub Actions for continuous integration and testing.

## Workflows

### 1. **E2E Tests** (`.github/workflows/e2e-tests.yml`)
Runs end-to-end tests against a real GCS bucket.

**Triggers:**
- Pull request creation/updates
- Push to `main` or `develop` branches
- Manual dispatch

**Requirements:**
- `GCP_SERVICE_ACCOUNT_KEY` secret with valid GCP credentials
- `GCS_TEST_BUCKET` secret with test bucket name

**Steps:**
1. Builds gcscfuse from source
2. Installs dependencies via vcpkg
3. Runs unit tests
4. Authenticates to GCP
5. Executes all E2E test scripts in `tests/e2e/`
6. Comments results on PR
7. Uploads artifacts

### 2. **CI Build** (`.github/workflows/ci.yml`)
Builds and validates code on multiple platforms.

**Triggers:**
- Pull request creation/updates
- Push to `main` or `develop` branches
- Manual dispatch

**Matrix:**
- OS: Ubuntu 22.04, Ubuntu 20.04
- Build Type: Release, Debug

**Steps:**
1. Builds on each matrix configuration
2. Runs unit tests
3. Performs code quality checks (clang-format, cppcheck)
4. Uploads build artifacts

## Setup Instructions

### 1. Configure Repository Secrets

Go to your repository → Settings → Secrets and variables → Actions

Add the following secrets:

#### `GCP_SERVICE_ACCOUNT_KEY`
Service account JSON key with permissions:
- `storage.buckets.get`
- `storage.objects.create`
- `storage.objects.delete`
- `storage.objects.get`
- `storage.objects.list`

```bash
# Create service account
gcloud iam service-accounts create gcscfuse-ci \
    --display-name="gcscfuse CI Testing"

# Grant permissions
gcloud projects add-iam-policy-binding YOUR_PROJECT_ID \
    --member="serviceAccount:gcscfuse-ci@YOUR_PROJECT_ID.iam.gserviceaccount.com" \
    --role="roles/storage.objectAdmin"

# Create and download key
gcloud iam service-accounts keys create key.json \
    --iam-account=gcscfuse-ci@YOUR_PROJECT_ID.iam.gserviceaccount.com

# Copy the entire contents of key.json to GCP_SERVICE_ACCOUNT_KEY secret
cat key.json
```

#### `GCS_TEST_BUCKET`
Name of the GCS bucket for testing (without `gs://` prefix):
```
my-gcsfuse-test-bucket
```

### 2. Create Test Bucket

```bash
# Create bucket
gsutil mb -p YOUR_PROJECT_ID gs://YOUR_TEST_BUCKET/

# Set lifecycle to auto-delete old test files
cat > lifecycle.json <<EOF
{
  "lifecycle": {
    "rule": [
      {
        "action": {"type": "Delete"},
        "condition": {"age": 7}
      }
    ]
  }
}
EOF

gsutil lifecycle set lifecycle.json gs://YOUR_TEST_BUCKET/
```

### 3. Enable Required APIs

```bash
gcloud services enable storage-api.googleapis.com
gcloud services enable iam.googleapis.com
```

## Running Workflows Locally

### Test E2E locally:
```bash
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json
export TEST_BUCKET=your-test-bucket

cd tests/e2e
python3 test_cache_ttl.py $TEST_BUCKET
```

### Test build locally:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
ctest --output-on-failure
```

## Workflow Features

### Caching
- vcpkg dependencies are cached to speed up builds (~60min → ~5min)
- Cache key includes CMakeLists.txt hash for automatic invalidation

### Artifacts
- Build binaries are uploaded for 7 days
- Test results and logs are preserved
- Download from Actions tab → Workflow run → Artifacts

### PR Comments
E2E workflow automatically comments on PRs with:
- ✅/❌ Test status
- Link to detailed run
- Summary of results

### Parallel Execution
- CI build runs on multiple OS versions simultaneously
- Controlled vcpkg concurrency to avoid OOM

## Troubleshooting

### Build timeout (>60 min)
- Increase `timeout-minutes` in workflow
- Check vcpkg cache is working
- Use `VCPKG_MAX_CONCURRENCY=2` for low-memory runners

### GCP authentication fails
- Verify `GCP_SERVICE_ACCOUNT_KEY` is valid JSON
- Check service account has correct permissions
- Ensure APIs are enabled

### E2E tests fail
- Check `GCS_TEST_BUCKET` exists and is accessible
- Verify service account permissions
- Review test logs in artifacts

### Cache issues
- Manually delete cache: Settings → Actions → Caches
- Update cache key in workflow file
- Clear local `~/.cache/vcpkg`

## Status Badges

Add to README.md:

```markdown
[![E2E Tests](https://github.com/YOUR_USERNAME/gcscfuse/actions/workflows/e2e-tests.yml/badge.svg)](https://github.com/YOUR_USERNAME/gcscfuse/actions/workflows/e2e-tests.yml)
[![CI Build](https://github.com/YOUR_USERNAME/gcscfuse/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/gcscfuse/actions/workflows/ci.yml)
```

## Security Notes

- Service account keys should have minimal permissions
- Use Workload Identity Federation for better security (alternative to keys)
- Test bucket should be separate from production
- Enable bucket lifecycle policies to prevent cost accumulation
- Never commit service account keys to repository
