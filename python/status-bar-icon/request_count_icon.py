from icon_constants import STATUS_ICON_HEIGHT


class RequestCountStatusIcon:
    def __init__(self, requests_today, text_font, status_font_size):
        self.requests_today = max(0, int(requests_today))
        self.text_font = text_font
        self.status_font_size = status_font_size
        self.icon_height = STATUS_ICON_HEIGHT
        self.label = f"RPD {self.requests_today}"

    def measure(self):
        text_bbox = self.text_font.getbbox(self.label)
        text_w = text_bbox[2] - text_bbox[0]
        return (max(1, text_w), self.icon_height)

    def get_top_y(self):
        return self.status_font_size // 2

    def render(self, draw, x, y):
        ascent, descent = self.text_font.getmetrics()
        text_y = y + (self.icon_height - (ascent + descent)) // 2
        draw.text((x, text_y), self.label, font=self.text_font, fill="white")
