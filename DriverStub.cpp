// Licensed under the Apache License, Version 2.0, with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Driver.h"
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace llvm;

namespace {

class Pipe {
public:
  Pipe(llvm::raw_ostream *output) : output(output) {}
  ~Pipe() {
    if (fd[0] != -1)
      close(fd[0]);
    if (fd[1] != -1)
      close(fd[1]);
  }

  bool open() { return pipe(fd) == 0; }
  int getRead() const { return fd[0]; }
  int getWrite() const { return fd[1]; }
  void closeWrite() {
    close(fd[1]);
    fd[1] = -1;
  }
  void flush() const {
    constexpr size_t size = 1024;
    char buf[size];
    ssize_t ret;
    do {
      ret = read(getRead(), buf, size);
      if (ret > 0 && output)
        output->write(buf, ret);
    } while (ret == size || (ret == -1 && errno == EINTR));
  }

private:
  int fd[2] = {-1, -1};
  llvm::raw_ostream *output;
};

} // anonymous namespace

static bool InvokeLld(const char *variant, llvm::ArrayRef<const char *> args,
                      llvm::raw_ostream *stdoutOS,
                      llvm::raw_ostream *stderrOS) {
  Pipe outPipe(stdoutOS), errPipe(stderrOS);
  if (!outPipe.open())
    return false;
  if (!errPipe.open())
    return false;

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, outPipe.getWrite(), STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, errPipe.getWrite(), STDERR_FILENO);

  // We need the command as argument, and end with nullptr.
  SmallVector<char *, 32> arguments{const_cast<char *>(variant)};
  for (size_t arg = 1; arg != args.size(); ++arg)
    arguments.push_back(const_cast<char *>(args[arg]));
  arguments.push_back(nullptr);

  pid_t pid;
  if (posix_spawnp(&pid, variant, &actions, nullptr, arguments.data(),
                   environ) != 0)
    return false;
  posix_spawn_file_actions_destroy(&actions);
  outPipe.closeWrite();
  errPipe.closeWrite();

  // Wait for child to finish and flush pipes.
  sigset_t signals, old;
  sigemptyset(&signals);
  sigaddset(&signals, SIGCHLD);
  pthread_sigmask(SIG_BLOCK, &signals, &old);
  pollfd pollfds[2] = {{outPipe.getRead(), POLLIN},
                       {errPipe.getRead(), POLLIN}};
  int wstatus;
  do {
    poll(pollfds, 2, -1);
    if (pollfds[0].revents & POLLIN)
      outPipe.flush();
    if (pollfds[1].revents & POLLIN)
      errPipe.flush();
  } while (waitpid(pid, &wstatus, WNOHANG) == 0);
  pthread_sigmask(SIG_SETMASK, &old, nullptr);

  outPipe.flush();
  errPipe.flush();

  return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
}

static bool InvokeLld(const char *variant, llvm::ArrayRef<const char *> args,
                      llvm::raw_ostream &stdoutOS, llvm::raw_ostream &stderrOS,
                      bool disableOutput) {
  return InvokeLld(variant, args, disableOutput ? nullptr : &stdoutOS,
                   disableOutput ? nullptr : &stderrOS);
}

namespace lld {

namespace coff {
bool link(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,
          llvm::raw_ostream &stderrOS, bool exitEarly, bool disableOutput) {
  return InvokeLld("lld-link", args, stdoutOS, stderrOS, disableOutput);
}
} // namespace coff

namespace elf {
bool link(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,
          llvm::raw_ostream &stderrOS, bool exitEarly, bool disableOutput) {
  return InvokeLld("ld.lld", args, stdoutOS, stderrOS, disableOutput);
}
} // namespace elf

namespace macho {
bool link(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,
          llvm::raw_ostream &stderrOS, bool exitEarly, bool disableOutput) {
  return InvokeLld("ld64.lld", args, stdoutOS, stderrOS, disableOutput);
}
} // namespace macho

namespace wasm {
bool link(llvm::ArrayRef<const char *> args, llvm::raw_ostream &stdoutOS,
          llvm::raw_ostream &stderrOS, bool exitEarly, bool disableOutput) {
  return InvokeLld("wasm-ld", args, stdoutOS, stderrOS, disableOutput);
}
} // namespace wasm

} // namespace lld
