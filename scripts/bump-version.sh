#!/bin/bash
# scripts/bump-version.sh
# Usage: ./scripts/bump-version.sh <component> <type>
# component: annealer | python | shell
# type: major | minor | patch
set -e

if [ $# -ne 2 ]; then
    echo "Usage: $0 <component> <type>"
    echo "  component: annealer | python | shell"
    echo "  type: major | minor | patch"
    echo ""
    echo "Examples:"
    echo "  $0 annealer patch"
    echo "  $0 python minor"
    exit 1
fi

COMPONENT=$1
TYPE=$2

if [[ ! "$COMPONENT" =~ ^(annealer|python|shell)$ ]]; then
    echo "Error: component must be 'annealer', 'python', or 'shell'"
    exit 1
fi

if [[ ! "$TYPE" =~ ^(major|minor|patch)$ ]]; then
    echo "Error: type must be 'major', 'minor', or 'patch'"
    exit 1
fi

if [ "$COMPONENT" = "annealer" ]; then
    CURRENT=$(grep '#define VERSION' annealer/src/annealer.c | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
elif [ "$COMPONENT" = "python" ]; then
    CURRENT=$(grep '^VERSION = ' guardian/guardian.py | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
else
    CURRENT=$(grep '^VERSION=' scripts/sweep.sh | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
fi

IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"

case $TYPE in
    major) NEW="$((MAJOR+1)).0.0" ;;
    minor) NEW="$MAJOR.$((MINOR+1)).0" ;;
    patch) NEW="$MAJOR.$MINOR.$((PATCH+1))" ;;
esac

echo ""
echo "Component: $COMPONENT"
echo "Current version: $CURRENT"
echo "New version: $NEW ($TYPE bump)"
echo ""
read -p "Proceed with version bump? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Version bump cancelled"
    exit 0
fi

echo "Updating files..."

if [ "$COMPONENT" = "annealer" ]; then
    sed -i "s/#define VERSION \"[^\"]*\"/#define VERSION \"$NEW\"/" annealer/src/annealer.c
    git add annealer/src/annealer.c
    git commit -m "chore(annealer): bump version to $NEW"
    git tag "annealer-v$NEW"
elif [ "$COMPONENT" = "python" ]; then
    sed -i "s/^VERSION = \"[^\"]*\"/VERSION = \"$NEW\"/" guardian/guardian.py
    sed -i "s/^VERSION = \"[^\"]*\"/VERSION = \"$NEW\"/" mutator/mutator.py
    sed -i "s/^VERSION = \"[^\"]*\"/VERSION = \"$NEW\"/" bench/bench.py
    git add guardian/guardian.py mutator/mutator.py bench/bench.py
    git commit -m "chore(python): bump version to $NEW"
    git tag "python-v$NEW"
else
    sed -i "s/^VERSION=\"[^\"]*\"/VERSION=\"$NEW\"/" scripts/sweep.sh
    sed -i "s/^VERSION=\"[^\"]*\"/VERSION=\"$NEW\"/" docker/entrypoint.sh
    git add scripts/sweep.sh docker/entrypoint.sh
    git commit -m "chore(shell): bump version to $NEW"
    git tag "shell-v$NEW"
fi

echo ""
echo "$COMPONENT version bumped to $NEW"
echo "Git tag created: ${COMPONENT}-v$NEW"
echo ""
echo "Next steps:"
echo "  git push"
echo "  git push --tags"

