import AVFoundation

public class AudioEngineController 
{
    let engine = AVAudioEngine()
    private let m_bridge: TheNonagonBridge

    public init(bridge: TheNonagonBridge)
    {
        self.m_bridge = bridge
        setupAudio()
    }

    private func setupAudio()
    {
        let sourceNode = AVAudioSourceNode { [weak self] _, timestamp, frameCount, audioBufferList in
            guard let self = self else { return noErr }
            
            let abl = UnsafeMutableAudioBufferListPointer(audioBufferList)
            let numChannels = abl.count
            let numFrames = Int(frameCount)

            var channelPointers = [UnsafeMutablePointer<Float>?](repeating: nil, count: numChannels)
            
            for i in 0..<numChannels 
            {
                let buffer = abl[i]
                channelPointers[i] = buffer.mData?.assumingMemoryBound(to: Float.self)
            }

            channelPointers.withUnsafeMutableBufferPointer { ptr in
                guard let baseAddress = ptr.baseAddress else { return }
                self.m_bridge.Process(baseAddress, Int32(numChannels), Int32(numFrames), timestamp.pointee)
            }

            return noErr
        }

        engine.attach(sourceNode)
        engine.connect(sourceNode, to: engine.outputNode, format: engine.outputNode.inputFormat(forBus: 0))

        do  
        {
            try engine.start()
        } 
        catch 
        {
            print("Audio failed to start: \(error)")
        }
    }

    public func start() 
    {
        // optional: restart engine or unpause
    }
}
