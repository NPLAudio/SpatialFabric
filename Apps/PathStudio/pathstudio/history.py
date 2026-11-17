"""Snapshot undo/redo for JSON-like documents (bounded, no GUI)."""
import copy


class History:
    def __init__(self, max_depth=100):
        self.max_depth = max_depth
        self.undo_stack, self.redo_stack = [], []

    def checkpoint(self, document):
        """Remember the state BEFORE a mutation; any redo history is dropped."""
        self.record(copy.deepcopy(document))

    def record(self, snapshot):
        """Push an already-copied pre-mutation snapshot (ownership transfers to the history)."""
        self.undo_stack.append(snapshot)
        del self.undo_stack[:-self.max_depth]
        self.redo_stack.clear()

    def can_undo(self):
        return bool(self.undo_stack)

    def can_redo(self):
        return bool(self.redo_stack)

    def undo(self, current):
        if not self.undo_stack:
            return None
        self.redo_stack.append(copy.deepcopy(current))
        return self.undo_stack.pop()

    def redo(self, current):
        if not self.redo_stack:
            return None
        self.undo_stack.append(copy.deepcopy(current))
        return self.redo_stack.pop()

    def clear(self):
        self.undo_stack.clear()
        self.redo_stack.clear()
