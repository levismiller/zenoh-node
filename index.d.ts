/**
 * A Zenoh session in peer mode.
 *
 * @example
 * const { Session } = require('zenoh-node')
 * const session = new Session()                        // peer, multicast discovery
 * const session = new Session('tcp/192.168.1.10:7447') // peer, explicit locator
 * session.put('my/key/expr', JSON.stringify(payload))
 * session.close()
 */
export declare class Session {
  /**
   * Open a Zenoh session in peer mode.
   * @param locator  Optional endpoint to connect to (e.g. "tcp/192.168.1.10:7447").
   *                 Omit to rely on multicast peer discovery.
   */
  constructor(locator?: string)

  /**
   * Publish a message on a key expression.
   * @param keyexpr  Zenoh key expression (e.g. "fortem/auth/user/prefs")
   * @param payload  String payload
   */
  put(keyexpr: string, payload: string): void

  /** Close the session and release all resources. */
  close(): void
}
