export class Epoll {
  constructor(
    callback: (
      err: Error | null,
      fs: number | undefined,
      events: number | undefined
    ) => void
  );

  get closed(): boolean;

  add(fd: number, events: number): Epoll;
  close(): void;
  remove(fd: number): Epoll;
  modify(fd: number, events: number): Epoll;

  static EPOLLIN: number;
  static EPOLLOUT: number;
  static EPOLLRDHUP: number;
  static EPOLLPRI: number;
  static EPOLLERR: number;
  static EPOLLHUP: number;
  static EPOLLET: number;
  static EPOLLONESHOT: number;
}

// TODO - should it export as possibly null?
// export const Epoll: EpollClass | null;
