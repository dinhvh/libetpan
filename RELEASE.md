# Release Checklist

Before tagging a release, update the release metadata and regenerate the
checked-in release artifacts.

1. Update release metadata in `configure.ac`.

   Review these fields:

   - `maj_version`
   - `min_version`
   - `mic_version`
   - `api_current`
   - `api_revision`
   - `api_compatibility`

   A practical workflow is to pass `configure.ac` to Codex or Claude and ask it
   to change `configure.ac` for the intended release version and API
   compatibility. Review the result before committing.

2. Regenerate the autotools output after changing `configure.ac`.

   ```sh
   ./autogen.sh
   ./configure
   ```

3. Refresh release artifacts.

   ```sh
   build-release/prepare-release.sh
   ```

   This refreshes:

   - `build-windows/libetpan_version.h`
   - `build-windows/build_headers.list`
   - `include/libetpan/` public headers for Swift Package Manager

4. Review the generated changes.

   ```sh
   git status
   git diff
   ```

5. Run the normal build and test matrix for the release.

6. Commit the release metadata and generated artifacts, then tag the release.
