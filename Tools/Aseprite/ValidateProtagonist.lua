local output=assert(app.params.output,'Missing output directory')
local function readJson(name)
    local file=assert(io.open(app.fs.joinPath(output,name),'r'))
    local value=json.decode(file:read('*a')); file:close(); return value
end
local rig=readJson('protagonist_rig.json')
local export=readJson('protagonist_walk_aseprite.json')
local sprite=app.open(app.fs.joinPath(output,'protagonist_walk.aseprite'))
assert(sprite and #sprite.frames==33,'Native source must contain idle + 32 walk frames')
assert(#sprite.layers==4,'Expected four editable body-part layers')
local tagRanges={}
for _,tag in ipairs(sprite.tags) do
    tagRanges[tag.name]={tag.fromFrame.frameNumber,tag.toFrame.frameNumber}
end
assert(tagRanges.idle and tagRanges.idle[1]==1 and tagRanges.idle[2]==1,'Invalid idle tag')
assert(tagRanges.walk and tagRanges.walk[1]==2 and tagRanges.walk[2]==33,'Invalid walk tag')
assert(#export.frames==32,'Export must contain exactly 32 walk frames')
local seen,images={},{}
local headReference=nil
local minFront,maxFront,minBack,maxBack=1e9,-1e9,1e9,-1e9
for i=1,32 do
    local image=Image(512,512)
    image:drawSprite(sprite,i+1,Point(0,0))
    local bytes=image.bytes
    assert(not seen[bytes],'Repeated walk pose: '..i)
    seen[bytes]=true; images[i]=image
    local head=Image(image,Rectangle(190,50,145,95)).bytes
    if headReference then assert(head==headReference,'Head artwork changed in frame '..i) end
    headReference=head
    local bounds=image:shrinkBounds()
    assert(bounds.x>=8 and bounds.y>=8 and bounds.x+bounds.width<=504 and
           bounds.y+bounds.height<=504,'Clipped frame: '..i)
    local frame=export.frames[i].frame
    assert(frame.w==512 and frame.h==512,'Trimmed or inconsistent frame geometry')
    minFront=math.min(minFront,rig.frames[i].frontFoot[1])
    maxFront=math.max(maxFront,rig.frames[i].frontFoot[1])
    minBack=math.min(minBack,rig.frames[i].backFoot[1])
    maxBack=math.max(maxBack,rig.frames[i].backFoot[1])
end
assert(maxFront-minFront>=75 and maxBack-minBack>=75,'Insufficient stride movement')
local changes,minChange,maxChange={},1e9,0
for i=1,32 do
    local nextImage=images[i%32+1]
    local changed=0
    for y=270,465 do for x=130,390 do
        if images[i]:getPixel(x,y)~=nextImage:getPixel(x,y) then changed=changed+1 end
    end end
    assert(changed>=64,'No visible leg change between frames '..i..' and '..(i%32+1))
    changes[i]=changed; minChange=math.min(minChange,changed); maxChange=math.max(maxChange,changed)
end
local maxInterior=0
for i=1,31 do maxInterior=math.max(maxInterior,changes[i]) end
assert(changes[32]<=maxInterior*1.5,'Abrupt last-to-first loop transition')
local result={passed=true,tool=tostring(app.version),walkFrames=32,uniqueFrames=32,
              canvasWidth=512,canvasHeight=512,frontStride=maxFront-minFront,
              backStride=maxBack-minBack,minAdjacentLegPixelsChanged=minChange,
              maxAdjacentLegPixelsChanged=maxChange,loopLegPixelsChanged=changes[32],
              checks={'native layers and tags','32 unique walk poses','constant canvas',
                      'transparent padding','unchanged head artwork','visible articulated legs',
                      'last-to-first change bounded by interior transitions'}}
local file=assert(io.open(app.fs.joinPath(output,'animation_validation.json'),'w'))
file:write(json.encode(result)); file:close()
print(json.encode(result))
