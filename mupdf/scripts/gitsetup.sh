#!/bin/bash

# Set up useful aliases:
git config alias.wsfix "! git diff HEAD >P.diff && git apply -R P.diff && git apply --whitespace=fix P.diff && rm P.diff"
git config alias.wsfixi "! git diff --cached >P.diff && git apply --cached -R P.diff && git apply --cached --whitespace=fix P.diff && rm P.diff"

# Copy hooks:
cp scripts/githooks/* .git/hooks/
