local Script = {}
function Script.OnCreate(self)
    self.count = 0
end
function Script.OnClick(self)
    self.count = self.count + 1
    self.entity:set_text(string.format("CLICKS %d", self.count))
end
return Script
