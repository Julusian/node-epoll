'use strict';

module.exports = (_ => {
  const osType = require('os').type();

  if (osType === 'Linux') {
    return require("pkg-prebuilds")(
      __dirname,
      require("./binding-options")
    );
  }

  console.warn(`Warning: epoll is built for Linux and not intended for usage on ${osType}.`);

  return {
    Epoll: {}
  };
})();

