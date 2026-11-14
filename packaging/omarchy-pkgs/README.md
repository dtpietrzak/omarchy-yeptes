# omarchy-pkgs recipe

`yeptes/` is the recipe to submit to
[omacom/omarchy-pkgs](https://github.com/omacom/omarchy-pkgs) as
`pkgbuilds/yeptes/`. It builds from the GitHub release tarball of the tag
`v<pkgver>`, so the tag must exist before `updpkgsums` can fill in the
checksum (it is `SKIP` until then). After the first release, the
`upstream.watch` in `.omarchy/package.json` picks up later `v*` releases on
its own.

`pkgbuild/PKGBUILD` at the repository root is the local one `bin/install`
uses; keep `pkgver`, `pkgdesc`, and `depends` the same in both.
