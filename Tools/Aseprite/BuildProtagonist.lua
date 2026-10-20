-- Run with the locally built Aseprite, not a standalone Lua interpreter.
-- Editable cutout animation: stable artwork + two articulated legs + counter-swinging arm.
local sourcePath = assert(app.params.source, 'Missing source image')
local output = assert(app.params.output, 'Missing output directory')
app.fs.makeAllDirectories(output)
local pc = app.pixelColor
local atlas = Image{fromFile=sourcePath}
assert(atlas.width == 1254 and atlas.height == 1254, 'Unexpected reference atlas dimensions')
local source = Image(atlas, Rectangle(0, 0, 418, 418))
-- Remove saturated red matte spill and nearly-transparent speckles from the generated reference.
-- The leather belt is dark red and does not match this spill threshold.
for it in source:pixels() do
    local p=it()
    if pc.rgbaA(p)<32 or (pc.rgbaR(p)>220 and pc.rgbaG(p)<80 and pc.rgbaB(p)<80) then it(0) end
end
source:saveAs(app.fs.joinPath(output, 'reference_idle.png'))
local width, height, offsetX, offsetY = 512, 512, 47, 48
local ground = 407
local count = 32

local backPolygon = {{192,220},{233,222},{231,252},{208,293},{183,348},
                     {185,380},{187,410},{141,412},{140,373},{153,336},{169,289},{183,252}}
local frontPolygon = {{231,235},{268,234},{268,267},{258,301},{258,349},
                      {268,376},{290,387},{289,409},{224,410},{222,377},{225,347},{226,301},{223,272}}
local armPolygon = {{177,115},{194,117},{189,145},{180,177},{175,202},
                    {184,216},{179,236},{157,237},{149,222},{154,200},{156,179},{163,150}}

local function inside(x, y, polygon)
    local result = false
    local j = #polygon
    for i=1,#polygon do
        local a, b = polygon[i], polygon[j]
        if (a[2]>y) ~= (b[2]>y) and x < (b[1]-a[1])*(y-a[2])/(b[2]-a[2])+a[1] then
            result = not result
        end
        j = i
    end
    return result
end

local back, front, arm = Image(418,418), Image(418,418), Image(418,418)
local body = Image(source)
for it in source:pixels() do
    local value = it()
    local x, y = it.x+.5, it.y+.5
    if inside(x,y,backPolygon) then back:drawPixel(it.x,it.y,value); body:drawPixel(it.x,it.y,0) end
    if inside(x,y,frontPolygon) then front:drawPixel(it.x,it.y,value); body:drawPixel(it.x,it.y,0) end
    if inside(x,y,armPolygon) then arm:drawPixel(it.x,it.y,value); body:drawPixel(it.x,it.y,0) end
end

-- Bilinear sampling in premultiplied alpha avoids dark fringes on the cutout edges.
local function sample(image,x,y)
    local ix,iy = math.floor(x),math.floor(y)
    if ix<0 or iy<0 or ix+1>=image.width or iy+1>=image.height then return 0 end
    local fx,fy = x-ix,y-iy
    local colors = {image:getPixel(ix,iy),image:getPixel(ix+1,iy),image:getPixel(ix,iy+1),image:getPixel(ix+1,iy+1)}
    local weights = {(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy}
    local a,r,g,b = 0,0,0,0
    for i=1,4 do
        local alpha = pc.rgbaA(colors[i])*weights[i]
        a=a+alpha; r=r+pc.rgbaR(colors[i])*alpha
        g=g+pc.rgbaG(colors[i])*alpha; b=b+pc.rgbaB(colors[i])*alpha
    end
    if a<1 then return 0 end
    return pc.rgba(math.floor(r/a+.5),math.floor(g/a+.5),math.floor(b/a+.5),math.floor(a+.5))
end

local function distance(a,b)
    return math.sqrt((a[1]-b[1])^2+(a[2]-b[2])^2)
end
local function lerp(a,b,t) return a+(b-a)*t end

local function kneeAt(hip,ankle,l1,l2)
    local dx,dy=ankle[1]-hip[1],ankle[2]-hip[2]
    local d=math.sqrt(dx*dx+dy*dy)
    local stretch=math.max(1,(d+3)/(l1+l2))
    l1=l1*stretch; l2=l2*stretch
    local along=(l1*l1-l2*l2+d*d)/(2*d)
    local bend=math.sqrt(math.max(0,l1*l1-along*along))
    return {hip[1]+dx*along/d+dy*bend/d, hip[2]+dy*along/d-dx*bend/d}
end

local backRest={{211,222},{192,292},{168,354}}
local frontRest={{247,237},{242,295},{243,355}}
local function renderLeg(image,rest,phase,sole)
    local hip=rest[1]
    local ankle={hip[1]+38*math.cos(phase),ground-sole-26*math.max(0,-math.sin(phase))}
    local knee=kneeAt(hip,ankle,distance(rest[1],rest[2]),distance(rest[2],rest[3]))
    local result=Image(width,height)
    -- The piecewise inverse strip warp keeps both sides of the knee joined.
    for y=hip[2]-3,415 do
        local sy,sourceCenter,targetCenter
        if y<knee[2] then
            local t=(y-hip[2])/(knee[2]-hip[2])
            sy=lerp(rest[1][2],rest[2][2],t)
            sourceCenter=lerp(rest[1][1],rest[2][1],t)
            targetCenter=lerp(hip[1],knee[1],t)
        elseif y<ankle[2] then
            local t=(y-knee[2])/(ankle[2]-knee[2])
            sy=lerp(rest[2][2],rest[3][2],t)
            sourceCenter=lerp(rest[2][1],rest[3][1],t)
            targetCenter=lerp(knee[1],ankle[1],t)
        else
            sy=y-ankle[2]+rest[3][2]
            sourceCenter=rest[3][1]; targetCenter=ankle[1]
        end
        for x=105,335 do
            local value=sample(image,x-targetCenter+sourceCenter,sy)
            if pc.rgbaA(value)>0 then result:drawPixel(x+offsetX,y+offsetY,value) end
        end
    end
    return result,ankle,knee
end

local function renderArm(phase)
    local result=Image(width,height)
    local angle=.19*math.sin(phase)
    local c,s=math.cos(angle),math.sin(angle)
    local px,py=184,123
    for y=104,248 do for x=120,220 do
        local dx,dy=x-px,y-py
        local value=sample(arm,px+c*dx+s*dy,py-s*dx+c*dy)
        if pc.rgbaA(value)>0 then result:drawPixel(x+offsetX,y+offsetY,value) end
    end end
    return result
end

local function renderBody(phase)
    local result=Image(width,height)
    result:drawImage(Image(body,Rectangle(0,0,418,187)),Point(offsetX,offsetY))
    for y=187,417 do
        local sway=math.floor(3.5*math.sin(phase-.6)*(y-187)/230+.5)
        result:drawImage(Image(body,Rectangle(0,y,418,1)),Point(offsetX+sway,offsetY+y))
    end
    return result
end

local sprite=Sprite(width,height,ColorMode.RGB)
sprite:assignColorSpace(ColorSpace{ sRGB=true })
local backLayer=sprite.layers[1]; backLayer.name='Back leg'
local frontLayer=sprite:newLayer(); frontLayer.name='Front leg'
local bodyLayer=sprite:newLayer(); bodyLayer.name='Body and coat'
local armLayer=sprite:newLayer(); armLayer.name='Near arm'
sprite.frames[1].duration=.125
local idle=Image(width,height)
idle:drawImage(source,Point(offsetX,offsetY))
sprite:newCel(bodyLayer,1,idle,Point(0,0))
local metrics={schemaVersion=1,frameCount=count,width=width,height=height,columns=8,rows=4,
               bodyHeight=396,anchorX=267,anchorY=ground+offsetY,strideLength=144,
               tool=tostring(app.version),method='articulated cutout / inverse strip warp',frames={}}
for i=0,count-1 do
    local frame=sprite:newEmptyFrame(i+2)
    frame.duration=1/32
    local phase=i*2*math.pi/count
    local backImage,backFoot,backKnee=renderLeg(back,backRest,phase+math.pi,50)
    local frontImage,frontFoot,frontKnee=renderLeg(front,frontRest,phase,48)
    sprite:newCel(backLayer,i+2,backImage,Point(0,0))
    sprite:newCel(frontLayer,i+2,frontImage,Point(0,0))
    sprite:newCel(bodyLayer,i+2,renderBody(phase),Point(0,0))
    sprite:newCel(armLayer,i+2,renderArm(phase),Point(0,0))
    metrics.frames[#metrics.frames+1]={frame=i,backFoot=backFoot,frontFoot=frontFoot,
                                    backKnee=backKnee,frontKnee=frontKnee}
end
-- Add tags after inserting frames, otherwise Aseprite extends an end-of-timeline tag.
sprite:newTag(1,1).name='idle'
sprite:newTag(2,33).name='walk'
sprite:saveAs(app.fs.joinPath(output,'protagonist_walk.aseprite'))
app.command.ExportSpriteSheet{ui=false,askOverwrite=false,type=SpriteSheetType.ROWS,
    columns=8,tag='walk',textureFilename=app.fs.joinPath(output,'protagonist_walk_aseprite.png'),
    dataFilename=app.fs.joinPath(output,'protagonist_walk_aseprite.json'),
    dataFormat=SpriteSheetDataFormat.JSON_ARRAY,trim=false,trimSprite=false,
    mergeDuplicates=false,ignoreEmpty=false,openGenerated=false}
idle:saveAs(app.fs.joinPath(output,'protagonist_idle_aseprite.png'))
local file=assert(io.open(app.fs.joinPath(output,'protagonist_rig.json'),'w'))
file:write(json.encode(metrics)); file:close()
assert(#sprite.frames==33,'Expected idle + 32 walk frames')
print('Created layered Aseprite source, 32-frame walk atlas, idle, and rig metadata.')
