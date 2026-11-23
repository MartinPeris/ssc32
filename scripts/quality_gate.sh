#!/usr/bin/env bash
set -euo pipefail

repository_root="$(git rev-parse --show-toplevel)"
readonly repository_root
source_mode=--working-tree
update_coverage_baseline=false
for argument in "$@"; do
  case "${argument}" in
    --staged | --working-tree) source_mode="${argument}" ;;
    --update-coverage-baseline) update_coverage_baseline=true ;;
    *)
      printf 'Usage: %s [--staged|--working-tree] [--update-coverage-baseline]\n' "$0" >&2
      exit 2
      ;;
  esac
done
readonly source_mode update_coverage_baseline

if [[ "${source_mode}" == "--staged" && "${update_coverage_baseline}" == true ]]; then
  printf 'The coverage baseline can only be updated from the working tree.\n' >&2
  exit 2
fi

for command in docker git python3 tar; do
  if ! command -v "${command}" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "${command}" >&2
    exit 1
  fi
done

if ! docker info >/dev/null 2>&1; then
  printf 'Docker is required and must be running.\n' >&2
  exit 1
fi

if [[ "${source_mode}" == "--staged" ]]; then
  git -C "${repository_root}" diff --cached --check
fi

workspace="$(mktemp -d "${TMPDIR:-/tmp}/ssc32-quality.XXXXXX")"
cleanup() {
  rm -rf "${workspace}"
}
trap cleanup EXIT

quality_source="${workspace}/src/ssc32"
mkdir -p "${quality_source}"
if [[ "${source_mode}" == "--staged" ]]; then
  tree="$(git -C "${repository_root}" write-tree)"
  git -C "${repository_root}" archive "${tree}" |
    tar -xf - -C "${quality_source}"
else
  tar -C "${repository_root}" \
    --exclude=.git --exclude=build --exclude=install --exclude=log \
    -cf - . | tar -xf - -C "${quality_source}"
fi

dockerfile="${quality_source}/docker/Dockerfile.quality"
image_tag="ssc32-quality:$(git hash-object "${dockerfile}" | cut -c1-12)"
if ! docker image inspect "${image_tag}" >/dev/null 2>&1; then
  printf 'Building quality image %s (first run only for this toolchain revision)\n' "${image_tag}"
  docker build --tag "${image_tag}" --file "${dockerfile}" "${quality_source}"
fi

printf 'Running quality gate against the %s snapshot\n' "${source_mode#--}"
docker run --rm \
  --volume "${workspace}:/workspace" \
  --volume ssc32_ccache:/root/.cache/ccache \
  --env CCACHE_DIR=/root/.cache/ccache \
  "${image_tag}" \
  bash /workspace/src/ssc32/scripts/quality_gate_inner.sh

if [[ "${update_coverage_baseline}" == true ]]; then
  python3 "${repository_root}/scripts/coverage_ratchet.py" update \
    "${repository_root}/coverage-baseline.json" \
    "${workspace}/coverage-summary.json"
fi
