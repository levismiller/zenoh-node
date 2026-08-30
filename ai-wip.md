# WIP / Future Work

## GitHub Actions CI/CD for npm publish

Set up a matrix workflow to build prebuilds and publish to npm automatically on version bump or tag.

**Plan:**
- Use a matrix build: `ubuntu-latest` (x64) + `ubuntu-24.04-arm` (native arm64) runners
- Each runner builds its platform prebuild via `npm run build:prebuilds:x64` / `npm run build:prebuilds:arm64`
- A final job collects both artifacts and runs `npm publish --access public`
- Add `NPMJS_TOKEN` as a GitHub repo secret (the automation/granular token with 2FA bypass)

**Why matrix instead of Docker-in-Docker on one runner:**
- `build:prebuilds:arm64` under QEMU on x64 would take 30+ min
- Native arm64 GitHub runners (`ubuntu-24.04-arm`) are fast and available

## Multi-language zenoh clients

Need the same pub/sub capability in C++, Python, and Java. All use official zenoh clients that speak the same wire protocol — no changes to auth-service needed.

- **C++**: `zenoh-cpp` (same library under zenoh-node, header-only wrapper around zenoh-c)
- **Python**: `zenoh-python` — `pip install zenoh`
- **Java**: `zenoh-java` — Maven artifact `io.zenoh:zenoh-java`

Main thing to sort out per language: explicit TCP locator for Docker/production (same `tcp/<host>:7447` pattern already solved here).
