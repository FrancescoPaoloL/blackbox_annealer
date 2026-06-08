#!/bin/bash
# scripts/bump-version.sh
# Usage: ./scripts/bump-version.sh <component> <type>
# component: api | frontend
# type: major | minor | patch

set -e

# Validate arguments
if [ $# -ne 2 ]; then
    echo "Usage: $0 <component> <type>"
    echo "  component: api | frontend"
    echo "  type: major | minor | patch"
    echo ""
    echo "Examples:"
    echo "  $0 api patch"
    echo "  $0 frontend minor"
    exit 1
fi

COMPONENT=$1
TYPE=$2

# Validate component
if [[ ! "$COMPONENT" =~ ^(api|frontend)$ ]]; then
    echo "Error: component must be 'api' or 'frontend'"
    exit 1
fi

# Validate type
if [[ ! "$TYPE" =~ ^(major|minor|patch)$ ]]; then
    echo "Error: type must be 'major', 'minor', or 'patch'"
    exit 1
fi

# Get current version based on component
if [ "$COMPONENT" = "api" ]; then
    CURRENT=$(grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' api/__version__.py)
    VERSION_FILE="api/__version__.py"
else
    CURRENT=$(grep -o '"version": "[^"]*"' frontend/package.json | cut -d'"' -f4)
    VERSION_FILE="frontend/package.json"
fi

# Parse version
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"

# Calculate new version
case $TYPE in
    major)
        NEW="$((MAJOR+1)).0.0"
        ;;
    minor)
        NEW="$MAJOR.$((MINOR+1)).0"
        ;;
    patch)
        NEW="$MAJOR.$MINOR.$((PATCH+1))"
        ;;
esac

# Show changes and ask for confirmation
echo ""
echo "Component: $COMPONENT"
echo "Current version: $CURRENT"
echo "New version: $NEW ($TYPE bump)"
echo ""
read -p "Proceed with version bump? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "❌ Version bump cancelled"
    exit 0
fi

echo "Updating files..."

# Update files based on component
if [ "$COMPONENT" = "api" ]; then
    echo "__version__ = \"$NEW\"" > api/__version__.py
    git add api/__version__.py
    git commit -m "chore(api): bump version to $NEW"
    git tag "api-v$NEW"
else
    sed -i "s/\"version\": \"[^\"]*\"/\"version\": \"$NEW\"/" frontend/package.json
    sed -i "s/frontendVersion: '[^']*'/frontendVersion: '$NEW'/" frontend/views/index.ejs
    git add frontend/package.json frontend/views/index.ejs
    git commit -m "chore(frontend): bump version to $NEW"
    git tag "frontend-v$NEW"
fi

echo ""
echo "$COMPONENT version bumped to $NEW"
echo "Git tag created: ${COMPONENT}-v$NEW"
echo ""
echo "Next steps:"
echo "  git push"
echo "  git push --tags"

