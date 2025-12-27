#!/bin/bash
# Set up a 'venv' and run sphinx to build the docs!

python -m venv build/venv-docs
source build/venv-docs/bin/activate

pip install -r docs/requirements.txt

pip install sphinx_markdown_builder

sphinx-build -b markdown -d build/.doctrees docs build/markdown 2>&1 \
	| sed '/WARNING: more than one target found for .any. cross-reference.*:doc:.*:js:class:/d'

mkdir -p build/markdown/images
cp -f docs/images/* build/markdown/images
sed -i -e 's,(images/,(../images/,' build/markdown/*/*.md
sed -i -e 's,(images/,(../../images/,' build/markdown/*/*/*.md
sed -i -e 's,(images/,(../../../images/,' build/markdown/*/*/*/*.md
