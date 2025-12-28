# 🚀 Quick Start: CI/CD

## What You Get (Zero Setup!)

✅ **Automatic unit testing** on every PR  
✅ **Multi-platform builds** (Ubuntu 22.04 & 20.04)  
✅ **Build artifacts** for download  
✅ **Status badge** in README  
✅ **No GCP authentication required!**

## Usage (Works Immediately!)

### 1. Create a PR

```bash
git checkout -b my-feature
# Make your changes
git add .
git commit -m "Add my feature"
git push origin my-feature
gh pr create
```

### 2. CI Runs Automatically

The CI workflow will:
- ✅ Build on Ubuntu 22.04 & 20.04
- ✅ Test Release & Debug builds
- ✅ Run all unit tests
- ✅ Check code quality
- ✅ Update PR status

**No configuration needed!**

## Optional: E2E Tests

E2E tests require GCP authentication and are **manual-only**.

<details>
<summary>Click to expand E2E setup (optional)</summary>

### Setup GCP (One-time)

```bash
# 1. Create service account
export PROJECT_ID="your-project-id"
gcloud iam service-accounts create gcscfuse-ci
gcloud projects add-iam-policy-binding $PROJECT_ID \
  --member="serviceAccount:gcscfuse-ci@${PROJECT_ID}.iam.gserviceaccount.com" \
  --role="roles/storage.objectAdmin"
gcloud iam service-accounts keys create key.json \
  --iam-account="gcscfuse-ci@${PROJECT_ID}.iam.gserviceaccount.com"

# 2. Create test bucket
gsutil mb gs://gcscfuse-ci-test-${PROJECT_ID}

# 3. Configure GitHub secret
gh secret set GCP_SERVICE_ACCOUNT_KEY < key.json
rm key.json
```

### Run E2E Tests

**GitHub UI:**
1. Go to Actions → "E2E Tests (Manual)"
2. Click "Run workflow"
3. Enter your bucket name
4. Click "Run workflow"

**GitHub CLI:**
```bash
gh workflow run e2e-tests.yml -f test_bucket=gcscfuse-ci-test-${PROJECT_ID}
```

</details>

## Test Locally

```bash
# Quick validation
./.github/validate-local.sh

# Or build manually
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(nproc)
ctest --output-on-failure
```

## Monitor CI

- **GitHub UI:** Repository → Actions tab
- **CLI:** `gh run list` or `gh run watch`
- **PR status:** Shows ✅/❌ automatically

## Files Created

```
✅ .github/workflows/e2e-tests.yml    # E2E test workflow
✅ .github/workflows/ci.yml           # Build workflow
✅ .github/setup-ci.sh                # Setup script
✅ .github/validate-local.sh          # Local validation
✅ .github/CI_SETUP.md                # Full documentation
✅ .github/pull_request_template.md  # PR template
✅ README.md                          # Added CI badges
```

## What Happens on PR?

1. **CI Workflow Starts** automatically
2. **Build Matrix:** 4 parallel builds (2 OS × 2 build types)
3. **Unit Tests:** Run with CTest
4. **Code Quality:** clang-format, cppcheck
5. **Status Updated:** ✅ or ❌ on PR
6. **Artifacts:** Binary uploaded (optional download)

**No GCP authentication required!**

## Next Steps

� **Just start coding!** CI runs automatically  
🔧 Test locally with `./.github/validate-local.sh`  
📚 See [README.md](.github/README.md) for details  

---

**That's it!** No setup required. CI works out of the box! 🎉
