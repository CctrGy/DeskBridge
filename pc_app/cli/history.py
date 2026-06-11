"""In-memory CLI history."""

from dataclasses import dataclass, field


@dataclass(slots=True)
class History:
    entries: list[str] = field(default_factory=list)
    limit: int = 200
    visual_size: int = 25

    def add(self, command: str) -> None:
        if command:
            self.entries.append(command)
            if len(self.entries) > self.limit:
                del self.entries[: len(self.entries) - self.limit]

    def set_limit(self, value: int) -> None:
        self.limit = max(1, value)
        if len(self.entries) > self.limit:
            del self.entries[: len(self.entries) - self.limit]

    def set_visual_size(self, value: int) -> None:
        self.visual_size = max(1, value)

    def format(self) -> str:
        if not self.entries:
            return "Sin historial."
        return "\n".join(f"{index + 1:>3}  {entry}" for index, entry in enumerate(self.entries))
