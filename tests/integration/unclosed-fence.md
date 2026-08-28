# Unclosed Fence

The fence below is never closed. Everything after it is code, and the `---`
inside it must not split the deck.

```python
def present(markdown):
    return parse(markdown)

---

# This heading is inside the code block, not a new slide
