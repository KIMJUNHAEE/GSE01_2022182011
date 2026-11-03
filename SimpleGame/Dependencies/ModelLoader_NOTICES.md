# Model loader dependencies

Both dependencies are MIT licensed. Complete license texts are included beside the headers.

- cgltf, jkuhlmann/cgltf, commit `85cd62382dfea638278962690cf515023f33ed00`.
  [Upstream](https://github.com/jkuhlmann/cgltf/tree/85cd62382dfea638278962690cf515023f33ed00).
  Header SHA-256: `EFB169DEE911696B5D35FC8E3F7EA0C56D679DEBC529EBA9CA6AA6443BA9D5E9`.
  License: `cgltf.LICENSE`. Implementation is compiled once in `cgltf_impl.cpp`.
- JSON for Modern C++, nlohmann/json, release `v3.12.0`.
  [Upstream](https://github.com/nlohmann/json/tree/v3.12.0).
  Header SHA-256: `AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63`.
  License: `json.LICENSE`.

No Blender runtime, Assimp DLL or package manager is required to run the game loader.
Headers are vendored; normal game builds do not download network dependencies.
