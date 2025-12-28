#!/bin/bash
# CI Setup Script - Configures GitHub Actions secrets and test bucket
# Run this script to set up CI/CD for gcscfuse

set -e

echo "==========================================="
echo "gcscfuse CI/CD Setup"
echo "==========================================="
echo ""

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo "❌ GitHub CLI (gh) is not installed."
    echo "   Install it from: https://cli.github.com/"
    exit 1
fi

# Check if gcloud is installed
if ! command -v gcloud &> /dev/null; then
    echo "❌ Google Cloud SDK (gcloud) is not installed."
    echo "   Install it from: https://cloud.google.com/sdk/install"
    exit 1
fi

# Get project ID
echo "📋 Enter your GCP Project ID:"
read -r PROJECT_ID

if [ -z "$PROJECT_ID" ]; then
    echo "❌ Project ID cannot be empty"
    exit 1
fi

# Set project
gcloud config set project "$PROJECT_ID"

echo ""
echo "1️⃣  Creating service account..."
SERVICE_ACCOUNT_NAME="gcscfuse-ci"
SERVICE_ACCOUNT_EMAIL="${SERVICE_ACCOUNT_NAME}@${PROJECT_ID}.iam.gserviceaccount.com"

# Create service account
if gcloud iam service-accounts describe "$SERVICE_ACCOUNT_EMAIL" &>/dev/null; then
    echo "   ℹ️  Service account already exists"
else
    gcloud iam service-accounts create "$SERVICE_ACCOUNT_NAME" \
        --display-name="gcscfuse CI Testing" \
        --project="$PROJECT_ID"
    echo "   ✅ Service account created"
fi

echo ""
echo "2️⃣  Granting permissions..."
gcloud projects add-iam-policy-binding "$PROJECT_ID" \
    --member="serviceAccount:${SERVICE_ACCOUNT_EMAIL}" \
    --role="roles/storage.objectAdmin" \
    --condition=None
echo "   ✅ Permissions granted"

echo ""
echo "3️⃣  Creating service account key..."
KEY_FILE="gcscfuse-ci-key.json"
gcloud iam service-accounts keys create "$KEY_FILE" \
    --iam-account="$SERVICE_ACCOUNT_EMAIL" \
    --project="$PROJECT_ID"
echo "   ✅ Key created: $KEY_FILE"

echo ""
echo "4️⃣  Creating test bucket..."
echo "📋 Enter test bucket name (e.g., gcscfuse-ci-test-123):"
read -r BUCKET_NAME

if [ -z "$BUCKET_NAME" ]; then
    echo "❌ Bucket name cannot be empty"
    exit 1
fi

# Create bucket
if gsutil ls "gs://${BUCKET_NAME}" &>/dev/null; then
    echo "   ℹ️  Bucket already exists"
else
    gsutil mb -p "$PROJECT_ID" "gs://${BUCKET_NAME}"
    echo "   ✅ Bucket created"
fi

# Set lifecycle policy
cat > /tmp/lifecycle.json <<EOF
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

gsutil lifecycle set /tmp/lifecycle.json "gs://${BUCKET_NAME}"
rm /tmp/lifecycle.json
echo "   ✅ Lifecycle policy applied (auto-delete after 7 days)"

echo ""
echo "5️⃣  Configuring GitHub secrets..."

# Check if gh is authenticated
if ! gh auth status &>/dev/null; then
    echo "   🔐 Please authenticate with GitHub:"
    gh auth login
fi

# Set secrets
echo "   Setting GCP_SERVICE_ACCOUNT_KEY..."
gh secret set GCP_SERVICE_ACCOUNT_KEY < "$KEY_FILE"

echo "   Setting GCS_TEST_BUCKET..."
echo "$BUCKET_NAME" | gh secret set GCS_TEST_BUCKET

echo "   ✅ Secrets configured"

echo ""
echo "6️⃣  Enabling required APIs..."
gcloud services enable storage-api.googleapis.com
gcloud services enable iam.googleapis.com
echo "   ✅ APIs enabled"

echo ""
echo "==========================================="
echo "✅ CI/CD Setup Complete!"
echo "==========================================="
echo ""
echo "📋 Summary:"
echo "   • Project ID: $PROJECT_ID"
echo "   • Service Account: $SERVICE_ACCOUNT_EMAIL"
echo "   • Test Bucket: gs://$BUCKET_NAME"
echo "   • Key File: $KEY_FILE (keep secure!)"
echo ""
echo "🔒 Security Notes:"
echo "   • Delete $KEY_FILE after setup (optional but recommended)"
echo "   • The key is stored in GitHub Secrets"
echo "   • Test bucket will auto-delete files after 7 days"
echo ""
echo "🚀 Next Steps:"
echo "   1. Push your code to GitHub"
echo "   2. Create a Pull Request"
echo "   3. CI workflows will run automatically"
echo ""
echo "📚 Documentation:"
echo "   • CI Setup Guide: .github/CI_SETUP.md"
echo "   • E2E Tests: tests/e2e/"
echo ""
