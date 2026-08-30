/**
 * A Zenoh session in peer mode.
 *
 * @example
 * const { Session } = require('zenoh-node')
 * const session = new Session()                        // peer, multicast discovery
 * const session = new Session('tcp/192.168.1.10:7447') // peer, explicit locator
 */
export declare class Session {
  /**
   * Open a Zenoh session in peer mode.
   * @param locator  Optional endpoint to connect to (e.g. "tcp/192.168.1.10:7447").
   *                 Omit to rely on multicast peer discovery.
   */
  constructor(locator?: string | { connect?: string; listen?: string })

  /**
   * Publish a value on a key expression.
   */
  put(keyexpr: string, payload: string): void

  /**
   * Subscribe to values matching a key expression. Wildcards are supported (e.g. "fortem/auth/**").
   * The callback fires for each received value.
   */
  subscribe(keyexpr: string, callback: (keyexpr: string, payload: string) => void): void

  /**
   * Declare a queryable — respond to get() requests from other peers.
   * The callback receives the queried key expression, query parameters, and a reply function.
   * Call reply(payload) exactly once per query to send the response.
   */
  declareQueryable(
    keyexpr: string,
    callback: (keyexpr: string, params: string, reply: (payload: string) => void) => void
  ): void

  /**
   * Query matching queryables in the network.
   * onReply is called for each reply received.
   * onDone is called once when all replies have been received.
   */
  get(
    keyexpr: string,
    params: string,
    onReply: (keyexpr: string, payload: string) => void,
    onDone?: () => void
  ): void

  /** Close the session and release all resources. */
  close(): void
}
