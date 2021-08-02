/**
 * Returns a promise that resolves after the specified amount of time.
 * @param ms Time in milliseconds until the returned promise resolves
 * @returns `Promise` that will resolve after at least `ms` milliseconds
 */
export function sleep(ms: number): Promise<void> {
    return new Promise(resolve => {
        setTimeout(resolve, ms);
    });
}
