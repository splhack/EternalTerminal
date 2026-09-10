#include "FdPoller.hpp"
#include "TcpSocketHandler.hpp"
#include "TestHeaders.hpp"

using namespace et;

TEST_CASE("FdPoller supports descriptors above FD_SETSIZE", "[FdPoller]") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  int highFd = fcntl(pipeFds[0], F_DUPFD, FD_SETSIZE);
  REQUIRE(highFd >= FD_SETSIZE);
  REQUIRE(close(pipeFds[0]) == 0);
  int highWriteFd = fcntl(pipeFds[1], F_DUPFD, FD_SETSIZE);
  REQUIRE(highWriteFd >= FD_SETSIZE);
  REQUIRE(close(pipeFds[1]) == 0);

  FdPoller poller;
  poller.setFds({highFd});

  char byte = 'x';
  CHECK(isSocketWritable(highWriteFd));
  REQUIRE(write(highWriteFd, &byte, 1) == 1);
  CHECK(waitOnSocketData(highFd));
  TcpSocketHandler socketHandler;
  CHECK(socketHandler.waitForData(highFd, 0, 0));
  CHECK(poller.wait(1000).readable.count(highFd) == 1);

  CHECK(close(highFd) == 0);
  CHECK(close(highWriteFd) == 0);
}

TEST_CASE("FdPoller replaces its descriptor set", "[FdPoller]") {
  int firstPipe[2];
  int secondPipe[2];
  REQUIRE(pipe(firstPipe) == 0);
  REQUIRE(pipe(secondPipe) == 0);

  FdPoller poller;
  poller.setFds({firstPipe[0]});
  CHECK(poller.wait(0).readable.empty());

  poller.setFds({secondPipe[0]});
  char byte = 'x';
  REQUIRE(write(secondPipe[1], &byte, 1) == 1);
  FdPoller::Ready ready = poller.wait(1000);
  CHECK(ready.readable.count(firstPipe[0]) == 0);
  CHECK(ready.readable.count(secondPipe[0]) == 1);

  CHECK(close(firstPipe[0]) == 0);
  CHECK(close(firstPipe[1]) == 0);
  CHECK(close(secondPipe[0]) == 0);
  CHECK(close(secondPipe[1]) == 0);
}

TEST_CASE("FdPoller reports write readiness", "[FdPoller]") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);

  FdPoller poller;
  poller.setFds({pipeFds[0]}, {pipeFds[1]});

  FdPoller::Ready ready = poller.wait(1000);
  // An empty pipe is writable but not readable.
  CHECK(ready.writable.count(pipeFds[1]) == 1);
  CHECK(ready.readable.count(pipeFds[0]) == 0);

  char byte = 'x';
  REQUIRE(write(pipeFds[1], &byte, 1) == 1);
  ready = poller.wait(1000);
  CHECK(ready.readable.count(pipeFds[0]) == 1);

  CHECK(close(pipeFds[0]) == 0);
  CHECK(close(pipeFds[1]) == 0);
}

TEST_CASE("FdPoller refreshes a reused descriptor number", "[FdPoller]") {
  int firstPipe[2];
  int secondPipe[2];
  REQUIRE(pipe(firstPipe) == 0);
  REQUIRE(pipe(secondPipe) == 0);

  int watchedFd = firstPipe[0];
  FdPoller poller;
  poller.setFds({watchedFd});

  REQUIRE(close(watchedFd) == 0);
  REQUIRE(dup2(secondPipe[0], watchedFd) == watchedFd);
  REQUIRE(close(secondPipe[0]) == 0);
  poller.setFds({watchedFd}, {}, {watchedFd});

  char byte = 'x';
  REQUIRE(write(secondPipe[1], &byte, 1) == 1);
  CHECK(poller.wait(1000).readable.count(watchedFd) == 1);

  CHECK(close(watchedFd) == 0);
  CHECK(close(firstPipe[1]) == 0);
  CHECK(close(secondPipe[1]) == 0);
}

TEST_CASE("Socket polling surfaces invalid descriptors", "[FdPoller]") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  REQUIRE(close(pipeFds[0]) == 0);

  CHECK(waitOnSocketData(pipeFds[0], 0, 0));

  CHECK(close(pipeFds[1]) == 0);
}
